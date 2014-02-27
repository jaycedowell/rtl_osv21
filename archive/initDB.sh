#!/bin/bash

rm wx-data.db
sqlite3 wx-data.db < wx-data.sql
