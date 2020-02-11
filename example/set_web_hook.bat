set BOT_TOKEN=%1
set BOT_URL=%2

curl "https://api.telegram.org/bot%BOT_TOKEN%/setWebhook?url=%BOT_URL%"
pause