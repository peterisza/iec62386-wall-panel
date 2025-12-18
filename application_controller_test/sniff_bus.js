#!/usr/bin/env node
/**
 * DALI bus sniffing script for hasseb USB DALI Master
 * Enables sniffing mode and prints 16-bit packets received from the DALI bus
 */

const HID = require('node-hid');

// Hasseb device identifiers
const HASSEB_VENDOR_ID = 0x04cc;
const HASSEB_PRODUCT_ID = 0x0802;

// Protocol constants
const PREAMBLE = 0xAA;
const CMD_CONFIGURE_DEVICE = 0x05;
const MODE_SNIFFING = 0x01;
const REPORT_TYPE_TRANSMISSION = 0x07;
const SNIFFING_BYTE = 0x05;
const SNIFFING_BYTE_ERROR = 0x06;
const FRAME_LENGTH_16_BITS = 16;

let device = null;
let frameCount = 0;

/**
 * Format a date to readable timestamp
 */
function getTimestamp() {
    const now = new Date();
    const ms = now.getMilliseconds().toString().padStart(3, '0');
    return now.toISOString().replace('T', ' ').substring(0, 19) + '.' + ms;
}

/**
 * Format bytes as hex string
 */
function formatHex(bytes) {
    return bytes.map(b => `0x${b.toString(16).toUpperCase().padStart(2, '0')}`).join(' ');
}

/**
 * Enable sniffing mode on the device
 */
function enableSniffing() {
    console.log('Enabling sniffing mode...');
    
    // Command 0x05: Configure device
    // Byte 0: Preamble (0xAA)
    // Byte 1: Command code (0x05)
    // Byte 2: Sequence number (1, can be any non-zero)
    // Byte 3: Mode (0x01 = Data sniffing mode)
    // Bytes 4-9: Reserved/zero
    
    const command = [
        PREAMBLE,           // Byte 0: Preamble
        CMD_CONFIGURE_DEVICE, // Byte 1: Command code
        1,                  // Byte 2: Sequence number
        MODE_SNIFFING,      // Byte 3: Mode (0x01 = sniffing)
        0, 0, 0, 0, 0, 0    // Bytes 4-9: Reserved
    ];
    
    try {
        device.write(command);
        console.log('Sniffing mode enabled');
        return true;
    } catch (error) {
        console.error(`Error enabling sniffing mode: ${error.message}`);
        return false;
    }
}

/**
 * Parse and handle transmission reports
 */
function handleTransmissionReport(data) {
    // Report structure:
    // Byte 0: Preamble (0xAA)
    // Byte 1: Report type (0x07 = Transmission report)
    // Byte 2: Sequence number
    // Byte 3: Report type (0x05 = Sniffing byte, 0x06 = Sniffing byte error)
    // Byte 4: Data length in bits
    // Byte 5-7: Data bytes
    
    if (data.length < 8) {
        return;
    }
    
    const transmissionReportType = data[3];
    const dataLength = data[4];
    const dataBytes = [data[5], data[6], data[7]];
    
    // Process 16-bit packets
    // Note: Some firmware versions might send dataLength as bytes (2) instead of bits (16)
    if (dataLength === FRAME_LENGTH_16_BITS || dataLength === 2) {
        frameCount++;
        const timestamp = getTimestamp();
        const byte1 = dataBytes[0];
        const byte2 = dataBytes[1];
        
        if (transmissionReportType === SNIFFING_BYTE) {
            console.log(`[${timestamp}] #${frameCount.toString().padStart(5, '0')} - 16-bit packet: ${formatHex([byte1, byte2])} (${byte1.toString(16).toUpperCase().padStart(2, '0')} ${byte2.toString(16).toUpperCase().padStart(2, '0')})`);
        } else if (transmissionReportType === SNIFFING_BYTE_ERROR) {
            console.log(`[${timestamp}] #${frameCount.toString().padStart(5, '0')} - 16-bit packet ERROR: ${formatHex([byte1, byte2])} (${byte1.toString(16).toUpperCase().padStart(2, '0')} ${byte2.toString(16).toUpperCase().padStart(2, '0')})`);
        }
    }
}

/**
 * Process incoming HID reports
 */
function processReport(data) {
    if (data.length === 0) {
        return;
    }
    
    if (data.length < 3) {
        return;
    }
    
    const preamble = data[0];
    const reportType = data[1];
    
    // Handle transmission reports (type 0x07)
    if (preamble === PREAMBLE && reportType === REPORT_TYPE_TRANSMISSION) {
        handleTransmissionReport(data);
    }
}

/**
 * Main function
 */
function main() {
    console.log('='.repeat(60));
    console.log('DALI Bus Sniffer (hasseb USB DALI Master)');
    console.log('='.repeat(60));
    
    // Find hasseb device
    const devices = HID.devices().filter(d => 
        d.vendorId === HASSEB_VENDOR_ID && d.productId === HASSEB_PRODUCT_ID
    );
    
    if (devices.length === 0) {
        console.error('No hasseb USB DALI Master device found!');
        process.exit(1);
    }
    
    console.log(`Device found: ${devices[0].manufacturer} ${devices[0].product}`);
    console.log(`Path: ${devices[0].path}`);
    
    // Open device
    try {
        device = new HID.HID(devices[0].path);
        console.log('Device opened successfully');
    } catch (error) {
        console.error(`Error opening device: ${error.message}`);
        process.exit(1);
    }
    
    // Enable sniffing mode
    if (!enableSniffing()) {
        device.close();
        process.exit(1);
    }
    
    console.log('Listening for 16-bit packets... (Press Ctrl+C to exit)');
    console.log('='.repeat(60));
    
    // Handle graceful shutdown (this will be set up in the read loop)
    
    // Read loop with timeout for better error handling
    let shouldContinue = true;
    process.on('SIGINT', () => {
        shouldContinue = false;
    });
    
    try {
        while (shouldContinue) {
            try {
                // readTimeout blocks for up to 1000ms waiting for data
                // Returns null if no data is available within the timeout
                const data = device.readTimeout(1000);
                if (data && data.length > 0) {
                    // Convert to array of numbers if it's a Buffer
                    const dataArray = Buffer.isBuffer(data) ? Array.from(data) : data;
                    processReport(dataArray);
                }
            } catch (readError) {
                // Timeout is expected, just continue
                if (readError.message && readError.message.includes('timeout')) {
                    continue;
                }
                // Other errors might indicate device disconnection
                if (readError.message && (
                    readError.message.includes('cannot read') ||
                    readError.message.includes('disconnected')
                )) {
                    console.error(`Device read error: ${readError.message}`);
                    console.log('Device may have been disconnected. Exiting...');
                    break;
                }
                throw readError;
            }
        }
    } catch (error) {
        console.error(`Unexpected error: ${error.message}`);
    } finally {
        console.log('\n' + '='.repeat(60));
        console.log(`Sniffing stopped. Total packets received: ${frameCount}`);
        console.log('='.repeat(60));
        if (device) {
            device.close();
        }
    }
}

// Run main function
main();
