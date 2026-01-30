#!/bin/bash
# AMI BIOS Analysis Helper Script
# This script helps identify module names and GUIDs for creating SREP configs

echo "=== AMI BIOS Analysis Helper ==="
echo "This script helps you create SREP configuration files for AMI BIOS"
echo ""

# Check if SREP.log exists
if [ ! -f "SREP.log" ]; then
    echo "Error: SREP.log not found!"
    echo "Please run SREP first to generate the log file with module information."
    exit 1
fi

echo "Found SREP.log. Analyzing loaded modules..."
echo ""

# Extract module list from log
echo "=== Loaded Modules ==="
grep -A 1000 "Listing All Loaded Modules" SREP.log | grep -B 1000 "End of Module List" | grep -E "^\s+\[" | sort

echo ""
echo "=== Common AMI Setup Module Names ==="
echo "Look for modules with these names in the list above:"
echo "  - Setup"
echo "  - TSE"
echo "  - SetupBrowser"
echo "  - SetupUtility"
echo "  - ClickBiosSetup (MSI boards)"
echo ""

echo "=== Next Steps ==="
echo "1. Identify your setup module name from the list above"
echo "2. Extract your BIOS image with UEFITool"
echo "3. Use IFRExtractor on the Setup module to find form GUIDs"
echo "4. Create a SREP_Config.cfg using the template in SREP_Config_AMI_Example.cfg"
echo "5. Replace placeholder GUIDs with actual GUIDs from IFRExtractor output"
echo ""
echo "For more information, see the AMI BIOS Support section in README.md"
