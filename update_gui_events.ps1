$filePath = "c:\or\windsurfProjects\MachineController\src\gui\SettingsWindow.cpp"
$content = Get-Content -Path $filePath -Raw

# 1. Replace GuiMessage events
$content = $content -replace 'eventQueue_.push\(GuiEvent\{GuiEventType::GuiMessage, "(.*?)"(?:, "(.*?)")?\}\);', 
@'
GuiEvent event;
event.keyword = "GuiMessage";
event.data = "$1";
event.target = "$2";
eventQueue_.push(event);
'@

# 2. Replace ParameterChange events
$content = $content -replace 'eventQueue_.push\(GuiEvent\{GuiEventType::ParameterChange\}\);', 
@'
GuiEvent event;
event.keyword = "ParameterChange";
event.data = "Settings applied";
eventQueue_.push(event);
'@

# 3. Replace SendCommunicationMessage events
$content = $content -replace 'eventQueue_.push\(GuiEvent\{GuiEventType::SendCommunicationMessage, (.*?), "(.*?)"\}\);', 
@'
GuiEvent event;
event.keyword = "SendCommunicationMessage";
event.data = $1;
event.target = "$2";
eventQueue_.push(event);
'@

# 4. Replace SendTestMessage events
$content = $content -replace 'eventQueue_.push\(GuiEvent\{GuiEventType::SendTestMessage, (.*?), "(.*?)", (.*?)\}\);', 
@'
GuiEvent event;
event.keyword = "SendCommunicationMessage";
event.data = $1;
event.target = "$2";
event.intValue = $3;
eventQueue_.push(event);
'@

Set-Content -Path $filePath -Value $content

Write-Host "Updated all GuiEvent instances in SettingsWindow.cpp"
