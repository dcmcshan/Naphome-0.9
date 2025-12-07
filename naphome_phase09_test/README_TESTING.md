# nap.local Testing

This directory contains tests to validate the nap.local web server functionality.

## Setup

1. **Install Node.js and npm** (if not already installed)
   ```bash
   # Check if installed
   node --version
   npm --version
   ```

2. **Install Playwright dependencies**
   ```bash
   npm install
   npx playwright install chromium
   ```

## Running Tests

### After Flashing Firmware

1. **Flash the firmware** to your ESP32-S3 device:
   ```bash
   idf.py -p /dev/cu.usbserial-120 flash
   ```

2. **Wait for device to boot** and connect to WiFi (check serial monitor)

3. **Run the automated test script**:
   ```bash
   ./test_nap_local.sh
   ```

   Or run Playwright directly:
   ```bash
   npx playwright test test_nap_local.js
   ```

### Manual Testing

You can also manually test by opening:
- **Main page**: http://nap.local
- **Status API**: http://nap.local/api/status
- **Demo trigger**: POST to http://nap.local/api/demo/run

## Test Coverage

The Playwright tests validate:

1. ✅ Main page loads at nap.local
2. ✅ Page title and key elements are present
3. ✅ JSON status API returns correct structure
4. ✅ Demo trigger endpoint works
5. ✅ mDNS resolution works (nap.local resolves)
6. ✅ Test status table is displayed
7. ✅ Auto-refresh JavaScript functionality

## Troubleshooting

### nap.local doesn't resolve

- Ensure your computer and ESP32-S3 are on the same WiFi network
- Check that mDNS is enabled on your network/router
- Try accessing via IP address instead (check serial monitor for IP)
- On some networks, you may need to use `.local` instead of just the hostname

### Tests timeout

- Device may still be booting - wait a bit longer
- WiFi may not be connected - check serial monitor
- Increase timeout in `playwright.config.js` if needed

### Playwright not found

```bash
npm install
npx playwright install chromium
```
