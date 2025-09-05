#!/usr/bin/env python3
import sys, xml.etree.ElementTree as ET
if len(sys.argv) < 3:
    print("usage: coverage_gate.py <coverage.xml> <min-line-rate-0.0-1.0>")
    sys.exit(2)
xml, minrate = sys.argv[1], float(sys.argv[2])
tree = ET.parse(xml)
root = tree.getroot()
line_rate = float(root.attrib.get("line-rate", "0"))
print(f"Line coverage: {line_rate:.3f}")
sys.exit(0 if line_rate >= minrate else 1)
