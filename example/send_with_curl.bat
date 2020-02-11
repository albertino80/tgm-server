set BOT_TOKEN=%1
set CHAT_ID=%2

curl "https://api.telegram.org/bot%BOT_TOKEN%/sendMessage?chat_id=%CHAT_ID%&text=Hello+from+%COMPUTERNAME%"
pause