del /Q *.cpp
del /Q *.h

xcopy /Y %LIB_UTILS%\utilsBase.*
xcopy /Y %LIB_UTILS%\utilsDBNavi.*
xcopy /Y %LIB_UTILS%\utilsWebHttpsReqSync.*

pause
