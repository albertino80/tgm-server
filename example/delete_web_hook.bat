set BOT_TOKEN=%1

curl "https://api.telegram.org/bot%BOT_TOKEN%/deleteWebhook"
pause