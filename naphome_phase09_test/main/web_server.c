/**
 * @file web_server.c
 * @brief HTTP Web Server Implementation with mDNS and Status Monitoring
 */

#include "web_server.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "mdns.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_task_wdt.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdio.h>
#include "cJSON.h"

static const char *TAG = "web_server";
static httpd_handle_t server_handle = NULL;

// Test status tracking
#define MAX_TESTS 12
typedef struct {
    int status;  // 0=PASS, 1=WARNING, 2=FAIL, 3=NOT_IMPLEMENTED
    char name[64];
    bool has_status;
} test_status_info_t;

static test_status_info_t test_statuses[MAX_TESTS] = {0};
static portMUX_TYPE test_status_mutex = portMUX_INITIALIZER_UNLOCKED;

// External function to trigger test suite
extern void run_test_suite(void *pvParameters);
extern volatile bool test_suite_triggered;

// Test descriptions from TODO
static const char* test_descriptions[MAX_TESTS] = {
    "ESP32-S3 System Initialization",
    "SHT30 Temperature/Humidity Sensor",
    "SGP30 VOC Sensor",
    "BH1750 Light Sensor",
    "SCD30 CO2 Sensor",
    "PCA9685 RGB LED Control",
    "WiFi Connectivity",
    "AWS IoT Core MQTT Connectivity",
    "ESP-SR Wake Word Detection",
    "IR Blaster Functionality",
    "Audio Output System (TPA3116D2)",
    "Sensor Telemetry Publishing"
};

void web_server_update_test_status(int test_num, int status, const char *test_name)
{
    if (test_num < 1 || test_num > MAX_TESTS) {
        return;
    }
    
    portENTER_CRITICAL(&test_status_mutex);
    int idx = test_num - 1;
    test_statuses[idx].status = status;
    if (test_name) {
        strncpy(test_statuses[idx].name, test_name, sizeof(test_statuses[idx].name) - 1);
        test_statuses[idx].name[sizeof(test_statuses[idx].name) - 1] = '\0';
    } else {
        strncpy(test_statuses[idx].name, test_descriptions[idx], sizeof(test_statuses[idx].name) - 1);
        test_statuses[idx].name[sizeof(test_statuses[idx].name) - 1] = '\0';
    }
    test_statuses[idx].has_status = true;
    portEXIT_CRITICAL(&test_status_mutex);
}

bool web_server_trigger_demo(void)
{
    // Check if demo is already running
    if (test_suite_triggered) {
        ESP_LOGW(TAG, "Demo already running, cannot start another");
        return false;
    }
    
    // Trigger the test suite
    test_suite_triggered = true;
    xTaskCreate(
        run_test_suite,
        "test_suite",
        8192,
        NULL,
        5,
        NULL
    );
    
    ESP_LOGI(TAG, "Demo/test suite triggered from web interface");
    return true;
}

// HTML page with embedded JavaScript for auto-refresh
static const char* status_html = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"<title>Naphome Status - nap.local</title>"
"<style>"
"* { margin: 0; padding: 0; box-sizing: border-box; }"
"body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #1a1a1a; color: #e0e0e0; padding: 20px; }"
".container { max-width: 1400px; margin: 0 auto; }"
"h1 { color: #4CAF50; margin-bottom: 10px; text-align: center; }"
".subtitle { text-align: center; color: #888; margin-bottom: 30px; }"
".grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; margin-bottom: 20px; }"
".card { background: #2a2a2a; border-radius: 8px; padding: 20px; box-shadow: 0 2px 8px rgba(0,0,0,0.3); }"
".card h2 { color: #4CAF50; margin-bottom: 15px; font-size: 1.2em; border-bottom: 2px solid #4CAF50; padding-bottom: 5px; }"
".stat { display: flex; justify-content: space-between; margin: 10px 0; padding: 8px; background: #1a1a1a; border-radius: 4px; }"
".stat-label { font-weight: 600; color: #bbb; }"
".stat-value { color: #4CAF50; font-family: 'Courier New', monospace; }"
".progress-bar { width: 100%; height: 20px; background: #1a1a1a; border-radius: 10px; overflow: hidden; margin: 5px 0; }"
".progress-fill { height: 100%; background: linear-gradient(90deg, #4CAF50, #8BC34A); transition: width 0.3s; }"
".progress-fill.warning { background: linear-gradient(90deg, #FF9800, #FFC107); }"
".progress-fill.danger { background: linear-gradient(90deg, #F44336, #E91E63); }"
".task-table { width: 100%; border-collapse: collapse; margin-top: 10px; }"
".task-table th { background: #1a1a1a; padding: 10px; text-align: left; color: #4CAF50; border-bottom: 2px solid #4CAF50; }"
".task-table td { padding: 8px; border-bottom: 1px solid #333; }"
".task-table tr:hover { background: #333; }"
".core-badge { display: inline-block; padding: 4px 8px; border-radius: 4px; font-size: 0.85em; margin-left: 5px; }"
".core-0 { background: #2196F3; color: white; }"
".core-1 { background: #FF9800; color: white; }"
".refresh-info { text-align: center; color: #666; margin-top: 20px; font-size: 0.9em; }"
"</style>"
"</head>"
"<body>"
"<div class='container'>"
"<h1>🤖 Naphome Status Dashboard</h1>"
"<p class='subtitle'>Real-time MCU Monitoring - nap.local</p>"
"<div class='grid'>"
"<div class='card'>"
"<h2>System Information</h2>"
"<div class='stat'><span class='stat-label'>Chip Model:</span><span class='stat-value' id='chip-model'>-</span></div>"
"<div class='stat'><span class='stat-label'>Cores:</span><span class='stat-value' id='cores'>-</span></div>"
"<div class='stat'><span class='stat-label'>Revision:</span><span class='stat-value' id='revision'>-</span></div>"
"<div class='stat'><span class='stat-label'>CPU Frequency:</span><span class='stat-value' id='cpu-freq'>-</span></div>"
"<div class='stat'><span class='stat-label'>Uptime:</span><span class='stat-value' id='uptime'>-</span></div>"
"</div>"
"<div class='card'>"
"<h2>Memory Usage</h2>"
"<div class='stat'><span class='stat-label'>Free Heap:</span><span class='stat-value' id='free-heap'>-</span></div>"
"<div class='stat'><span class='stat-label'>Largest Free Block:</span><span class='stat-value' id='largest-block'>-</span></div>"
"<div class='stat'><span class='stat-label'>Min Free Ever:</span><span class='stat-value' id='min-free'>-</span></div>"
"<div class='progress-bar'><div class='progress-fill' id='heap-progress' style='width: 0%'></div></div>"
"<div class='stat'><span class='stat-label'>PSRAM Free:</span><span class='stat-value' id='psram-free'>-</span></div>"
"<div class='stat'><span class='stat-label'>PSRAM Total:</span><span class='stat-value' id='psram-total'>-</span></div>"
"</div>"
"<div class='card'>"
"<h2>CPU Usage</h2>"
"<div class='stat'><span class='stat-label'>Core 0 Usage:</span><span class='stat-value' id='core0-usage'>-</span></div>"
"<div class='progress-bar'><div class='progress-fill' id='core0-progress' style='width: 0%'></div></div>"
"<div class='stat'><span class='stat-label'>Core 1 Usage:</span><span class='stat-value' id='core1-usage'>-</span></div>"
"<div class='progress-bar'><div class='progress-fill' id='core1-progress' style='width: 0%'></div></div>"
"</div>"
"</div>"
"<div class='card'>"
"<h2>Running Tasks</h2>"
"<table class='task-table'>"
"<thead>"
"<tr>"
"<th>Task Name</th>"
"<th>State</th>"
"<th>Priority</th>"
"<th>Stack High Water</th>"
"<th>Core</th>"
"</tr>"
"</thead>"
"<tbody id='task-list'>"
"<tr><td colspan='5' style='text-align: center;'>Loading...</td></tr>"
"</tbody>"
"</table>"
"</div>"
"<div class='card'>"
"<h2>Phase 0.9 Test Status</h2>"
"<div style='margin-bottom: 15px; text-align: center;'>"
"<button id='run-demo-btn' onclick='runDemo()' style='background: #4CAF50; color: white; border: none; padding: 12px 24px; border-radius: 6px; font-size: 16px; font-weight: bold; cursor: pointer; margin-bottom: 15px;'>🚀 Run Demo / Test Suite</button>"
"<div id='demo-status' style='color: #888; font-size: 0.9em; margin-bottom: 10px;'></div>"
"</div>"
"<div style='margin-bottom: 15px;'>"
"<div class='stat'><span class='stat-label'>Tests Passed:</span><span class='stat-value' id='tests-passed'>-</span></div>"
"<div class='stat'><span class='stat-label'>Tests Warning:</span><span class='stat-value' id='tests-warning'>-</span></div>"
"<div class='stat'><span class='stat-label'>Tests Failed:</span><span class='stat-value' id='tests-failed'>-</span></div>"
"<div class='stat'><span class='stat-label'>Not Implemented:</span><span class='stat-value' id='tests-notimpl'>-</span></div>"
"</div>"
"<table class='task-table'>"
"<thead>"
"<tr>"
"<th>#</th>"
"<th>Test Name</th>"
"<th>Status</th>"
"</tr>"
"</thead>"
"<tbody id='test-list'>"
"<tr><td colspan='3' style='text-align: center;'>Loading...</td></tr>"
"</tbody>"
"</table>"
"</div>"
"<div class='card' style='grid-column: 1 / -1;'>"
"<h2>📊 GitHub Activity Dashboard</h2>"
"<div id='github-loading' style='text-align: center; color: #888; padding: 20px;'>Loading GitHub activity...</div>"
"<div id='github-dashboard' style='display: none;'>"
"<div class='grid' style='grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); margin-bottom: 20px;'>"
"<div class='stat' style='flex-direction: column; text-align: center; background: #1a1a1a; padding: 15px; border-radius: 6px;'>"
"<div style='font-size: 2em; color: #4CAF50; font-weight: bold;' id='github-commits'>-</div>"
"<div style='color: #bbb; margin-top: 5px;'>Commits</div>"
"</div>"
"<div class='stat' style='flex-direction: column; text-align: center; background: #1a1a1a; padding: 15px; border-radius: 6px;'>"
"<div style='font-size: 2em; color: #2196F3; font-weight: bold;' id='github-prs'>-</div>"
"<div style='color: #bbb; margin-top: 5px;'>Pull Requests</div>"
"</div>"
"<div class='stat' style='flex-direction: column; text-align: center; background: #1a1a1a; padding: 15px; border-radius: 6px;'>"
"<div style='font-size: 2em; color: #FF9800; font-weight: bold;' id='github-issues'>-</div>"
"<div style='color: #bbb; margin-top: 5px;'>Issues</div>"
"</div>"
"<div class='stat' style='flex-direction: column; text-align: center; background: #1a1a1a; padding: 15px; border-radius: 6px;'>"
"<div style='font-size: 2em; color: #9C27B0; font-weight: bold;' id='github-repos'>-</div>"
"<div style='color: #bbb; margin-top: 5px;'>Repositories</div>"
"</div>"
"</div>"
"<div style='margin-top: 20px;'>"
"<h3 style='color: #4CAF50; margin-bottom: 10px; font-size: 1em;'>Recent Activity</h3>"
"<div id='github-activity-list' style='max-height: 300px; overflow-y: auto;'>"
"<div style='text-align: center; color: #888; padding: 20px;'>Loading activity...</div>"
"</div>"
"</div>"
"<div style='margin-top: 20px; padding: 15px; background: #1a1a1a; border-radius: 6px;'>"
"<h3 style='color: #4CAF50; margin-bottom: 10px; font-size: 1em;'>Contribution Heatmap (Last 30 Days)</h3>"
"<div id='github-heatmap' style='display: flex; flex-wrap: wrap; gap: 4px; justify-content: center;'>"
"</div>"
"</div>"
"</div>"
"</div>"
"<p class='refresh-info'>Auto-refreshing every 2 seconds | GitHub activity updates every 30 seconds</p>"
"</div>"
"<script>"
"function formatBytes(bytes) {"
"  if (bytes < 1024) return bytes + ' B';"
"  if (bytes < 1024*1024) return (bytes/1024).toFixed(2) + ' KB';"
"  return (bytes/(1024*1024)).toFixed(2) + ' MB';"
"}"
"function formatTime(seconds) {"
"  const days = Math.floor(seconds / 86400);"
"  const hours = Math.floor((seconds % 86400) / 3600);"
"  const mins = Math.floor((seconds % 3600) / 60);"
"  const secs = seconds % 60;"
"  if (days > 0) return days + 'd ' + hours + 'h ' + mins + 'm';"
"  if (hours > 0) return hours + 'h ' + mins + 'm ' + secs + 's';"
"  return mins + 'm ' + secs + 's';"
"}"
"function updateStatus() {"
"  fetch('/api/status')"
"    .then(response => response.json())"
"    .then(data => {"
"      // System Info"
"      document.getElementById('chip-model').textContent = data.system.chip_model || '-';"
"      document.getElementById('cores').textContent = data.system.cores || '-';"
"      document.getElementById('revision').textContent = data.system.revision || '-';"
"      document.getElementById('cpu-freq').textContent = (data.system.cpu_freq_mhz || 0) + ' MHz';"
"      document.getElementById('uptime').textContent = formatTime(data.system.uptime_seconds || 0);"
"      // Memory"
"      const freeHeap = data.memory.free_heap || 0;"
"      const totalHeap = data.memory.total_heap || 0;"
"      const heapUsed = totalHeap - freeHeap;"
"      const heapPercent = totalHeap > 0 ? (heapUsed / totalHeap * 100) : 0;"
"      document.getElementById('free-heap').textContent = formatBytes(freeHeap);"
"      document.getElementById('largest-block').textContent = formatBytes(data.memory.largest_free_block || 0);"
"      document.getElementById('min-free').textContent = formatBytes(data.memory.min_free_heap || 0);"
"      const heapProgress = document.getElementById('heap-progress');"
"      heapProgress.style.width = heapPercent + '%';"
"      if (heapPercent > 80) heapProgress.className = 'progress-fill danger';"
"      else if (heapPercent > 60) heapProgress.className = 'progress-fill warning';"
"      else heapProgress.className = 'progress-fill';"
"      document.getElementById('psram-free').textContent = formatBytes(data.memory.psram_free || 0);"
"      document.getElementById('psram-total').textContent = formatBytes(data.memory.psram_total || 0);"
"      // CPU Usage"
"      const core0Usage = data.cpu.core0_usage || 0;"
"      const core1Usage = data.cpu.core1_usage || 0;"
"      document.getElementById('core0-usage').textContent = core0Usage.toFixed(1) + '%';"
"      document.getElementById('core1-usage').textContent = core1Usage.toFixed(1) + '%';"
"      const core0Progress = document.getElementById('core0-progress');"
"      const core1Progress = document.getElementById('core1-progress');"
"      core0Progress.style.width = core0Usage + '%';"
"      core1Progress.style.width = core1Usage + '%';"
"      if (core0Usage > 80) core0Progress.className = 'progress-fill danger';"
"      else if (core0Usage > 60) core0Progress.className = 'progress-fill warning';"
"      else core0Progress.className = 'progress-fill';"
"      if (core1Usage > 80) core1Progress.className = 'progress-fill danger';"
"      else if (core1Usage > 60) core1Progress.className = 'progress-fill warning';"
"      else core1Progress.className = 'progress-fill';"
"      // Tasks"
"      const taskList = document.getElementById('task-list');"
"      if (data.tasks && data.tasks.length > 0) {"
"        taskList.innerHTML = data.tasks.map(task => {"
"          const stateNames = {'Running': '🟢 Running', 'Ready': '🟡 Ready', 'Blocked': '🔴 Blocked', 'Suspended': '⚪ Suspended'};"
"          const state = stateNames[task.state] || task.state;"
"          const coreBadge = task.core_id >= 0 ? `<span class='core-badge core-${task.core_id}'>Core ${task.core_id}</span>` : '<span class=\"core-badge\">Any</span>';"
"          return `<tr>"
"            <td>${task.name || 'Unknown'}</td>"
"            <td>${state}</td>"
"            <td>${task.priority || '-'}</td>"
"            <td>${formatBytes(task.stack_high_water || 0)}</td>"
"            <td>${coreBadge}</td>"
"          </tr>`;"
"        }).join('');"
"      } else {"
"        taskList.innerHTML = '<tr><td colspan=\"5\" style=\"text-align: center;\">No tasks found</td></tr>';"
"      }"
"      // Test Status"
"      if (data.tests) {"
"        let passCount = 0, warnCount = 0, failCount = 0, notImplCount = 0;"
"        const testList = document.getElementById('test-list');"
"        if (data.tests.length > 0) {"
"          testList.innerHTML = data.tests.map((test, idx) => {"
"            const statusNames = {"
"              0: '<span style=\"color: #4CAF50; font-weight: bold;\">✅ PASS</span>',"
"              1: '<span style=\"color: #FF9800; font-weight: bold;\">⚠️ WARNING</span>',"
"              2: '<span style=\"color: #F44336; font-weight: bold;\">❌ FAIL</span>',"
"              3: '<span style=\"color: #888; font-weight: bold;\">⚪ NOT IMPLEMENTED</span>'"
"            };"
"            const status = statusNames[test.status] || '<span>Unknown</span>';"
"            if (test.status === 0) passCount++;"
"            else if (test.status === 1) warnCount++;"
"            else if (test.status === 2) failCount++;"
"            else if (test.status === 3) notImplCount++;"
"            return `<tr>"
"              <td>${idx + 1}</td>"
"              <td>${test.name || 'Test ' + (idx + 1)}</td>"
"              <td>${status}</td>"
"            </tr>`;"
"          }).join('');"
"        } else {"
"          testList.innerHTML = '<tr><td colspan=\"3\" style=\"text-align: center;\">No test data available</td></tr>';"
"        }"
"        document.getElementById('tests-passed').textContent = passCount;"
"        document.getElementById('tests-warning').textContent = warnCount;"
"        document.getElementById('tests-failed').textContent = failCount;"
"        document.getElementById('tests-notimpl').textContent = notImplCount;"
"      }"
"    })"
"    .catch(error => {"
"      console.error('Error fetching status:', error);"
"    });"
"}"
"function runDemo() {"
"  const btn = document.getElementById('run-demo-btn');"
"  const status = document.getElementById('demo-status');"
"  btn.disabled = true;"
"  btn.textContent = 'Starting Demo...';"
"  status.textContent = 'Triggering demo/test suite...';"
"  fetch('/api/demo/run', { method: 'POST' })"
"    .then(response => response.json())"
"    .then(data => {"
"      if (data.success) {"
"        status.textContent = '✅ Demo started! Tests are running...';"
"        status.style.color = '#4CAF50';"
"        setTimeout(() => {"
"          btn.disabled = false;"
"          btn.textContent = '🚀 Run Demo / Test Suite';"
"          status.textContent = '';"
"        }, 5000);"
"      } else {"
"        status.textContent = '⚠️ ' + (data.message || 'Demo already running or failed to start');"
"        status.style.color = '#FF9800';"
"        btn.disabled = false;"
"        btn.textContent = '🚀 Run Demo / Test Suite';"
"      }"
"    })"
"    .catch(error => {"
"      status.textContent = '❌ Error: ' + error.message;"
"      status.style.color = '#F44336';"
"      btn.disabled = false;"
"      btn.textContent = '🚀 Run Demo / Test Suite';"
"    });"
"}"
"// Update immediately and then every 2 seconds"
"updateStatus();"
"let githubUpdateInterval = null;"
"function updateGitHubActivity() {"
"  fetch('/api/github')"
"    .then(response => response.json())"
"    .then(data => {"
"      if (data.error) {"
"        document.getElementById('github-loading').textContent = 'GitHub data unavailable: ' + data.error;"
"        return;"
"      }"
"      document.getElementById('github-loading').style.display = 'none';"
"      document.getElementById('github-dashboard').style.display = 'block';"
"      "
"      // Update stats"
"      document.getElementById('github-commits').textContent = data.commits || 0;"
"      document.getElementById('github-prs').textContent = data.pull_requests || 0;"
"      document.getElementById('github-issues').textContent = data.issues || 0;"
"      document.getElementById('github-repos').textContent = data.repositories || 0;"
"      "
"      // Update activity list"
"      const activityList = document.getElementById('github-activity-list');"
"      if (data.recent_activity && data.recent_activity.length > 0) {"
"        activityList.innerHTML = data.recent_activity.map(activity => {"
"          const date = new Date(activity.date).toLocaleDateString();"
"          const icon = activity.type === 'commit' ? '💾' : activity.type === 'pr' ? '🔀' : activity.type === 'issue' ? '📝' : '⭐';"
"          return `<div style='padding: 10px; margin: 5px 0; background: #2a2a2a; border-radius: 4px; border-left: 3px solid #4CAF50;'>"
"            <div style='display: flex; justify-content: space-between; align-items: center;'>"
"              <div><span style='font-size: 1.2em; margin-right: 8px;'>${icon}</span><strong>${activity.title}</strong></div>"
"              <div style='color: #888; font-size: 0.9em;'>${date}</div>"
"            </div>"
"            ${activity.repo ? `<div style='color: #bbb; font-size: 0.85em; margin-top: 5px; margin-left: 28px;'>${activity.repo}</div>` : ''}"
"          </div>`;"
"        }).join('');"
"      } else {"
"        activityList.innerHTML = '<div style=\"text-align: center; color: #888; padding: 20px;\">No recent activity</div>';"
"      }"
"      "
"      // Update heatmap"
"      const heatmap = document.getElementById('github-heatmap');"
"      if (data.heatmap && data.heatmap.length > 0) {"
"        heatmap.innerHTML = data.heatmap.map(day => {"
"          const intensity = Math.min(day.count / 10, 1);"
"          const opacity = 0.3 + (intensity * 0.7);"
"          const color = day.count === 0 ? '#161b22' : day.count < 3 ? '#0e4429' : day.count < 6 ? '#006d32' : day.count < 10 ? '#26a641' : '#39d353';"
"          return `<div style='width: 12px; height: 12px; background: ${color}; border-radius: 2px; opacity: ${opacity};' title='${day.date}: ${day.count} contributions'></div>`;"
"        }).join('');"
"      } else {"
"        heatmap.innerHTML = '<div style=\"text-align: center; color: #888; padding: 10px;\">No contribution data available</div>';"
"      }"
"    })"
"    .catch(error => {"
"      console.error('GitHub activity fetch error:', error);"
"      document.getElementById('github-loading').textContent = 'Failed to load GitHub activity';"
"    });"
"}"
"setInterval(updateStatus, 2000);"
"// Update GitHub activity every 30 seconds"
"updateGitHubActivity();"
"if (!githubUpdateInterval) {"
"  githubUpdateInterval = setInterval(updateGitHubActivity, 30000);"
"}"
"</script>"
"</body>"
"</html>";

// Handler for root path - serve HTML page
static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, status_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Handler for /api/demo/run - trigger demo
static esp_err_t api_demo_run_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        httpd_resp_set_status(req, "405 Method Not Allowed");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    
    bool success = web_server_trigger_demo();
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", success);
    if (!success) {
        cJSON_AddStringToObject(response, "message", "Demo already running or failed to start");
    } else {
        cJSON_AddStringToObject(response, "message", "Demo started successfully");
    }
    
    char *json_string = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string, strlen(json_string));
    
    free(json_string);
    cJSON_Delete(response);
    
    return ESP_OK;
}

// Handler for /api/status - return JSON status
static esp_err_t api_status_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *system = cJSON_CreateObject();
    cJSON *memory = cJSON_CreateObject();
    cJSON *cpu = cJSON_CreateObject();
    cJSON *tasks = cJSON_CreateArray();
    cJSON *tests = cJSON_CreateArray();

    // System information
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    const char* chip_model = "ESP32-S3";
    cJSON_AddStringToObject(system, "chip_model", chip_model);
    cJSON_AddNumberToObject(system, "cores", chip_info.cores);
    cJSON_AddNumberToObject(system, "revision", chip_info.revision);
    // Get CPU frequency (use default for now - can be read from sdkconfig)
    cJSON_AddNumberToObject(system, "cpu_freq_mhz", 240);  // Default 240MHz for ESP32-S3
    
    // Get uptime (use FreeRTOS tick count)
    cJSON_AddNumberToObject(system, "uptime_seconds", xTaskGetTickCount() * portTICK_PERIOD_MS / 1000);

    // Memory information
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free = esp_get_minimum_free_heap_size();
    size_t largest_free = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    // Get total heap size (free + allocated)
    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_DEFAULT);
    size_t total_heap = heap_info.total_free_bytes + heap_info.total_allocated_bytes;
    
    cJSON_AddNumberToObject(memory, "free_heap", free_heap);
    cJSON_AddNumberToObject(memory, "total_heap", total_heap);
    cJSON_AddNumberToObject(memory, "largest_free_block", largest_free);
    cJSON_AddNumberToObject(memory, "min_free_heap", min_free);
    
    // PSRAM information (if available)
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    cJSON_AddNumberToObject(memory, "psram_free", psram_free);
    cJSON_AddNumberToObject(memory, "psram_total", psram_total);

    // CPU usage (simplified - calculate based on idle task runtime)
    // Note: This is a simplified calculation. For accurate CPU usage, 
    // you'd need to enable FreeRTOS run-time stats
    float core0_usage = 0.0f;
    float core1_usage = 0.0f;
    
    // Get task information
    // Note: Detailed task info requires CONFIG_FREERTOS_USE_TRACE_FACILITY
    // For now, we'll create a simplified task list
    UBaseType_t num_tasks = uxTaskGetNumberOfTasks();
    
    // Create a basic task entry showing system is running
    // In a full implementation, you'd enable FreeRTOS trace facility
    // and use uxTaskGetSystemState() to get detailed task info
    cJSON *task = cJSON_CreateObject();
    cJSON_AddStringToObject(task, "name", "System");
    cJSON_AddStringToObject(task, "state", "Running");
    cJSON_AddNumberToObject(task, "priority", 1);
    cJSON_AddNumberToObject(task, "stack_high_water", 0);
    cJSON_AddNumberToObject(task, "core_id", 0);
    cJSON_AddItemToArray(tasks, task);
    
    // Add entry showing total task count
    task = cJSON_CreateObject();
    char task_count_str[32];
    snprintf(task_count_str, sizeof(task_count_str), "Total Tasks: %d", num_tasks);
    cJSON_AddStringToObject(task, "name", task_count_str);
    cJSON_AddStringToObject(task, "state", "-");
    cJSON_AddNumberToObject(task, "priority", 0);
    cJSON_AddNumberToObject(task, "stack_high_water", 0);
    cJSON_AddNumberToObject(task, "core_id", -1);
    cJSON_AddItemToArray(tasks, task);
    
    cJSON_AddNumberToObject(cpu, "core0_usage", core0_usage);
    cJSON_AddNumberToObject(cpu, "core1_usage", core1_usage);

    // Test status information
    portENTER_CRITICAL(&test_status_mutex);
    for (int i = 0; i < MAX_TESTS; i++) {
        cJSON *test = cJSON_CreateObject();
        cJSON_AddNumberToObject(test, "test_num", i + 1);
        if (test_statuses[i].has_status) {
            cJSON_AddStringToObject(test, "name", test_statuses[i].name);
            cJSON_AddNumberToObject(test, "status", test_statuses[i].status);
        } else {
            cJSON_AddStringToObject(test, "name", test_descriptions[i]);
            cJSON_AddNumberToObject(test, "status", 3);  // 3 = TEST_STATUS_NOT_IMPLEMENTED
        }
        cJSON_AddItemToArray(tests, test);
    }
    portEXIT_CRITICAL(&test_status_mutex);

    cJSON_AddItemToObject(root, "system", system);
    cJSON_AddItemToObject(root, "memory", memory);
    cJSON_AddItemToObject(root, "cpu", cpu);
    cJSON_AddItemToObject(root, "tasks", tasks);
    cJSON_AddItemToObject(root, "tests", tests);

    char *json_string = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string, strlen(json_string));
    
    free(json_string);
    cJSON_Delete(root);
    
    return ESP_OK;
}

// HTTP client event handler for GitHub API requests
static esp_err_t github_http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            if (!evt->user_data) {
                break;
            }
            // Append data to buffer
            strncat((char*)evt->user_data, (char*)evt->data, evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

// Handler for /api/github - return GitHub activity data
static esp_err_t api_github_handler(httpd_req_t *req)
{
    // For now, return mock data structure
    // In a full implementation, this would fetch from GitHub API
    // Using GraphQL or REST API with authentication
    
    cJSON *root = cJSON_CreateObject();
    
    // Mock GitHub activity data (replace with real API call)
    // TODO: Implement GitHub GraphQL API call using esp_http_client
    cJSON_AddNumberToObject(root, "commits", 42);
    cJSON_AddNumberToObject(root, "pull_requests", 8);
    cJSON_AddNumberToObject(root, "issues", 5);
    cJSON_AddNumberToObject(root, "repositories", 12);
    
    // Recent activity array
    cJSON *recent_activity = cJSON_CreateArray();
    
    // Add some mock recent activities
    cJSON *activity1 = cJSON_CreateObject();
    cJSON_AddStringToObject(activity1, "type", "commit");
    cJSON_AddStringToObject(activity1, "title", "Fixed MP3 playback stack overflow");
    cJSON_AddStringToObject(activity1, "repo", "Naphome-0.9");
    cJSON_AddStringToObject(activity1, "date", "2024-12-06T22:00:00Z");
    cJSON_AddItemToArray(recent_activity, activity1);
    
    cJSON *activity2 = cJSON_CreateObject();
    cJSON_AddStringToObject(activity2, "type", "pr");
    cJSON_AddStringToObject(activity2, "title", "Added GitHub activity dashboard");
    cJSON_AddStringToObject(activity2, "repo", "Naphome-0.9");
    cJSON_AddStringToObject(activity2, "date", "2024-12-06T21:30:00Z");
    cJSON_AddItemToArray(recent_activity, activity2);
    
    cJSON_AddItemToObject(root, "recent_activity", recent_activity);
    
    // Heatmap data (last 30 days)
    cJSON *heatmap = cJSON_CreateArray();
    // Generate mock heatmap data
    for (int i = 29; i >= 0; i--) {
        cJSON *day = cJSON_CreateObject();
        // Generate date string (simplified)
        char date_str[32];
        snprintf(date_str, sizeof(date_str), "2024-12-%02d", 7 - i);
        cJSON_AddStringToObject(day, "date", date_str);
        // Random contribution count (0-15)
        int count = (i % 7 == 0) ? 5 + (i % 3) : (i % 3);
        cJSON_AddNumberToObject(day, "count", count);
        cJSON_AddItemToArray(heatmap, day);
    }
    cJSON_AddItemToObject(root, "heatmap", heatmap);
    
    char *json_string = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string, strlen(json_string));
    
    free(json_string);
    cJSON_Delete(root);
    
    return ESP_OK;
}

// Web server task - runs in separate thread
static void web_server_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Web server task started");
    
    // Wait a bit for network to be fully ready
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    if (server_handle != NULL) {
        ESP_LOGW(TAG, "Web server already running");
        vTaskDelete(NULL);
        return;
    }

    // Initialize mDNS
    esp_err_t ret = mdns_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "mDNS init failed: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }

    // Set hostname
    mdns_hostname_set("nap");
    mdns_instance_name_set("Naphome Status Server");

    // Add service
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);

    ESP_LOGI(TAG, "mDNS initialized: nap.local");

    // Configure HTTP server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 10;
    config.max_open_sockets = 7;

    ret = httpd_start(&server_handle, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        mdns_free();
        vTaskDelete(NULL);
        return;
    }

    // Register URI handlers
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
    };
    httpd_register_uri_handler(server_handle, &root_uri);

    httpd_uri_t api_status_uri = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = api_status_handler,
    };
    httpd_register_uri_handler(server_handle, &api_status_uri);

    httpd_uri_t api_demo_run_uri = {
        .uri = "/api/demo/run",
        .method = HTTP_POST,
        .handler = api_demo_run_handler,
    };
    httpd_register_uri_handler(server_handle, &api_demo_run_uri);

    httpd_uri_t api_github_uri = {
        .uri = "/api/github",
        .method = HTTP_GET,
        .handler = api_github_handler,
    };
    httpd_register_uri_handler(server_handle, &api_github_uri);

    ESP_LOGI(TAG, "Web server started on port 80");
    
    // Get and log IP address
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK) {
            ESP_LOGI(TAG, "Access at: http://nap.local or http://" IPSTR, IP2STR(&ip_info.ip));
        }
    }

    // Task will continue running (server handles requests in background)
    // The HTTP server runs in its own task, so this task can just wait
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));  // Check every 10 seconds
        // Could add health checks here if needed
    }
}

esp_err_t web_server_start(void)
{
    if (server_handle != NULL) {
        ESP_LOGW(TAG, "Web server already running");
        return ESP_OK;
    }
    
    // Start web server in separate task
    BaseType_t ret = xTaskCreatePinnedToCore(
        web_server_task,
        "web_server",
        8192,  // Stack size
        NULL,
        3,     // Priority
        NULL,
        1      // Core 1 (separate from main tasks)
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create web server task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Web server task created");
    return ESP_OK;
}

esp_err_t web_server_stop(void)
{
    if (server_handle == NULL) {
        return ESP_OK;
    }

    httpd_stop(server_handle);
    server_handle = NULL;
    mdns_free();

    ESP_LOGI(TAG, "Web server stopped");
    return ESP_OK;
}
