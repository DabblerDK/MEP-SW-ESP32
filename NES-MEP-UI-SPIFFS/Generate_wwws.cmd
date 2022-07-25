@echo off
%~d1
cd "%~p1"
del update.wwws
for %%F in (www_*.*) do (
  echo /%%F>>update.wwws
  type %%F>>update.wwws
  echo.>>update.wwws
)
pause