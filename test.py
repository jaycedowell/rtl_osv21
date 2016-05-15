#!/usr/bin/env python

"""
Script to check whether any supported sensors can be detected
"""

from decoder import readRTL
from parser import parseBitStream

print("Gathering data for 90 seconds")
bits = readRTL(90)

print("Time's up, decoding the data stream")
output=parseBitStream(bits,verbose=True)
print(output)
