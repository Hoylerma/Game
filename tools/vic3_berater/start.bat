@echo off
rem Vic3-Berater unter Windows starten. Beim ersten Start wird eine virtuelle Umgebung angelegt.
rem Weitere Optionen einfach anhängen, z. B.:  start.bat --ziel "Großmacht werden" --auto 5
cd /d "%~dp0"
if not exist .venv\Scripts\python.exe (
    py -3 -m venv .venv || python -m venv .venv || goto :fehler
    .venv\Scripts\python.exe -m pip install --upgrade -r requirements.txt || goto :fehler
)
if "%ANTHROPIC_API_KEY%"=="" (
    echo ANTHROPIC_API_KEY ist nicht gesetzt. Siehe README.md, Abschnitt Einrichtung.
    pause
    exit /b 1
)
.venv\Scripts\python.exe berater.py %*
exit /b %errorlevel%

:fehler
echo Einrichtung fehlgeschlagen. Ist Python 3.10 oder neuer installiert (python.org, "Add to PATH")?
pause
exit /b 1
