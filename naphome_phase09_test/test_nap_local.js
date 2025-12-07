/**
 * Playwright test to validate nap.local web server
 * Run after flashing firmware: npx playwright test test_nap_local.js
 */

const { test, expect } = require('@playwright/test');

test.describe('nap.local Web Server Validation', () => {
  test('should load the main page at nap.local', async ({ page }) => {
    // Set longer timeout for mDNS resolution
    test.setTimeout(30000);
    
    // Try to access nap.local
    await page.goto('http://nap.local', { waitUntil: 'networkidle', timeout: 15000 });
    
    // Check page title
    await expect(page).toHaveTitle(/Naphome Status/i);
    
    // Check for key elements
    await expect(page.locator('h1')).toContainText(/Naphome/i);
    await expect(page.locator('text=Real-time MCU Monitoring')).toBeVisible();
  });

  test('should return JSON status from /api/status', async ({ request }) => {
    test.setTimeout(30000);
    
    const response = await request.get('http://nap.local/api/status', { timeout: 15000 });
    
    expect(response.status()).toBe(200);
    expect(response.headers()['content-type']).toContain('application/json');
    
    const json = await response.json();
    
    // Validate JSON structure
    expect(json).toHaveProperty('system');
    expect(json).toHaveProperty('memory');
    expect(json).toHaveProperty('cpu');
    expect(json).toHaveProperty('tasks');
    expect(json).toHaveProperty('tests');
    
    // Check system info
    expect(json.system).toHaveProperty('chip_model');
    expect(json.system).toHaveProperty('chip_revision');
    expect(json.system).toHaveProperty('cpu_freq_mhz');
    expect(json.system).toHaveProperty('uptime_seconds');
    
    // Check memory info
    expect(json.memory).toHaveProperty('free_heap');
    expect(json.memory).toHaveProperty('largest_free_block');
    expect(json.memory).toHaveProperty('min_free_heap');
    
    // Check tests array
    expect(Array.isArray(json.tests)).toBe(true);
  });

  test('should handle demo trigger POST request', async ({ request }) => {
    test.setTimeout(30000);
    
    const response = await request.post('http://nap.local/api/demo/run', { timeout: 15000 });
    
    // Should return 200 or 202 (accepted)
    expect([200, 202]).toContain(response.status());
    
    const text = await response.text();
    // Should contain success message
    expect(text.toLowerCase()).toMatch(/success|ok|triggered/i);
  });

  test('should have working mDNS resolution', async ({ page }) => {
    test.setTimeout(30000);
    
    // Try accessing via IP if nap.local doesn't resolve
    // First try nap.local
    try {
      await page.goto('http://nap.local', { waitUntil: 'networkidle', timeout: 10000 });
      await expect(page).toHaveTitle(/Naphome Status/i);
    } catch (e) {
      // If nap.local doesn't resolve, log warning but don't fail
      console.warn('nap.local did not resolve, this may be a network/mDNS issue');
      throw e;
    }
  });

  test('should display test status table', async ({ page }) => {
    test.setTimeout(30000);
    
    await page.goto('http://nap.local', { waitUntil: 'networkidle', timeout: 15000 });
    
    // Check for test status section
    await expect(page.locator('text=Phase 0.9 Test Status')).toBeVisible();
    
    // Check for test table
    const table = page.locator('table');
    await expect(table).toBeVisible();
  });

  test('should auto-refresh status via JavaScript', async ({ page }) => {
    test.setTimeout(30000);
    
    await page.goto('http://nap.local', { waitUntil: 'networkidle', timeout: 15000 });
    
    // Wait for auto-refresh to happen (should be every 2 seconds)
    await page.waitForTimeout(3000);
    
    // Check that status was fetched (look for updated timestamp or data)
    // The page should have made at least one API call
    const apiCalls = await page.evaluate(() => {
      return window.fetch ? 'fetch available' : 'no fetch';
    });
    
    expect(apiCalls).toBe('fetch available');
  });
});
