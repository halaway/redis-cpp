#!/bin/bash

# Define IP address and port of server to send data to
SERVER_IP="127.0.0.1"
SERVER_PORT=6379

# Define example 'ECHO' commands as string literal 
STRING_LITERAL="*2\r\n\$4\r\nECHO\r\n\$3\r\nhey\r\n"
#RAW_STRING_LITERAL="*2\r\n$4\r\nECHO\r\n$3\r\nhey\r\n"

# Defining 'SET' command 
SET_CMD_LITERAL="*3\r\n\$3\r\nSET\r\n\$3\r\ncar\r\n\$3\r\nbar\r\n"
GET_CMD_LITERAL="*2\r\n\$3\r\nGET\r\n\$3\r\ncar\r\n"

# Defining 'SET' command with expiration alloted time
SET_CMD_EXP="*5\r\n\$3\r\nSET\r\n\$3\r\nfoo\r\n\$3\r\nbar\r\n\$2\r\npx\r\n\$3\r\n100\r\n"
GET_CMD_EXP="*2\r\n\$3\r\nGET\r\n\$3\r\nfoo\r\n"

# Sending data  
#echo -e "$SET_CMD_LITERAL" | nc $SERVER_IP $SERVER_PORT
#echo -e "$GET_CMD_LITERAL" | nc $SERVER_IP $SERVER_PORT

# Sending data
echo -e "$SET_CMD_EXP" | nc $SERVER_IP $SERVER_PORT
echo -e "$GET_CMD_EXP" | nc $SERVER_IP $SERVER_PORT
