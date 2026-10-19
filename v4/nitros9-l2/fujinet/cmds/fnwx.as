********************************************************************
* fnwx - the first fnweather: FujiNet-powered weather TUI for NitrOS-9
*
* Uses FujiNet HTTP and JSON services to locate the user by public IP,
* then obtains current Open-Meteo conditions for the local area.
* Inspired by FujiNetWIFI/fujinet-weather.
*
* @license gpl v. 3, see fujinet/LICENSE for details.
*
* Edt/Rev  YYYY/MM/DD  Modified by
* Comment
* ------------------------------------------------------------------
*   1      2026/08/03  Andrew Diller / OpenAI
* Initial Wildbits Jr2 implementation.

stdin               equ       0
stdout              equ       1

UNIT$Metric         equ       0
UNIT$Imperial       equ       1
FORECASTS           equ       0
* Screen rows for the rule and the key help at the bottom: a Wildbits
* screen has 30 rows, the TFR/911H's terminal (ANSI=1) 24.
                    ifne      ANSI
RuleRow             equ       22
FootRow             equ       23
                    else
RuleRow             equ       23
FootRow             equ       25
                    endc
FCSZ                equ       48
FCDate              equ       0
FCCode              equ       12
FCLow               equ       17
FCHigh              equ       27
FCPrecip            equ       37

KEY$Enter           equ       C$CR

                    section   __os9
type                equ       Prgrm
lang                equ       Objct
attr                equ       ReEnt
rev                 equ       $00
edition             equ       1
stack               equ       512
                    endsect

                    section   bss
buspath             rmb       1
key                 rmb       1
signal              rmb       1
units               rmb       1
workidx             rmb       1
lastError           rmb       1
debugStep           rmb       2

oldopts             rmb       32
rawopts             rmb       32
                    ifne      ANSI
posbuf              rmb       9                   ESC [ row ; col H NUL
                    else
posbuf              rmb       3
                    endc
charbuf             rmb       1

city                rmb       33
region              rmb       33
country             rmb       4
latitude            rmb       20
longitude           rmb       20

datetime            rmb       21
timezone            rmb       13
temperature         rmb       13
feels               rmb       13
humidity            rmb       9
weathercode         rmb       5
windspeed           rmb       13
winddegree          rmb       7
pressure            rmb       13
forecast            rmb       FORECASTS*FCSZ

urlbuf              rmb       256
querybuf            rmb       40
linebuf             rmb       96
                    endsect

                    section   code

__start             clr       buspath,u
                    lbsr      TermInit
                    lbcs      ExitError
                    leax      debugStartup,pcr
                    lbsr      DebugSet
                    lbsr      FNNetInit
                    lbcs      RestoreError
                    sta       buspath,u
                    lbsr      FetchAll
                    lbcs      MainError

MainLoop            lbsr      DrawWeather
MainKey             lbsr      ReadKey
                    lbcs      MainDone
                    lda       key,u
                    cmpa      #'a
                    blo       MainCommon
                    cmpa      #'z
                    bhi       MainCommon
                    suba      #$20
MainCommon          cmpa      #'Q
                    lbeq      MainDone
                    cmpa      #'D
                    bne       MainRefresh
                    lbsr      DrawData
                    lbsr      ReadKey
                    lbra      MainLoop
MainRefresh
                    cmpa      #'R
                    bne       MainUnits
                    lbsr      FetchWeather
                    lbcs      MainError
                    bra       MainLoop
MainUnits           cmpa      #'U
                    bne       MainLocate
                    lda       units,u
                    eora      #1
                    sta       units,u
                    lbsr      FetchWeather
                    lbcs      MainError
                    bra       MainLoop
MainLocate          cmpa      #'L
                    bne       MainInfo
                    lbsr      FetchAll
                    lbcs      MainError
                    bra       MainLoop
MainInfo            cmpa      #'I
                    bne       MainHelp
                    lbsr      DrawInfo
                    lbsr      ReadKey
                    bra       MainLoop
MainHelp            cmpa      #'?
                    bne       MainKey
                    lbsr      DrawHelp
                    lbsr      ReadKey
                    bra       MainLoop

MainError           stb       lastError,u
                    lbsr      DrawError
ErrorKey            lbsr      ReadKey
                    lbcs      MainDone
                    lda       key,u
                    anda      #$DF
                    cmpa      #'Q
                    lbeq      MainDone
                    cmpa      #'R
                    bne       ErrorKey
                    lbsr      FetchAll
                    lbcs      MainError
                    lbra      MainLoop

MainDone            clrb
CloseExit           pshs      b,cc
                    lda       buspath,u
                    beq       RestoreExit
                    os9       I$Close
RestoreExit         lbsr      TermRestore
                    puls      b,cc
ExitError           os9       F$Exit
RestoreError        bra       CloseExit

********************************************************************
* Data acquisition

FetchAll            lbsr      FetchLocation
                    bcs       FetchAllExit
                    lbsr      AutoUnits
                    lbsr      FetchWeather
FetchAllExit        rts

FetchLocation       leax      loadingLocation,pcr
                    lbsr      DrawLoading
                    leax      debugLocateOpen,pcr
                    lbsr      DebugSet
                    leax      ipURL,pcr
                    lbsr      FNNetOpen
                    lbcs      FetchAbort
                    leax      debugLocateParse,pcr
                    lbsr      DebugSet
                    lbsr      FNNetParse
                    lbcs      FetchAbort
                    leax      debugLocateStatus,pcr
                    lbsr      DebugSet
                    leax      qStatus,pcr
                    leay      linebuf,u
                    ldb       #16
                    lbsr      FNNetQuery
                    lbcs      FetchAbort
                    leax      linebuf,u
                    leay      success,pcr
                    lbsr      StrEqual
                    bne       FetchLocationBad
                    leax      qCity,pcr
                    leay      city,u
                    ldb       #33
                    lbsr      FNNetQuery
                    lbcs      FetchAbort
                    leax      qRegion,pcr
                    leay      region,u
                    ldb       #33
                    lbsr      FNNetQuery
                    lbcs      FetchAbort
                    leax      qCountry,pcr
                    leay      country,u
                    ldb       #4
                    lbsr      FNNetQuery
                    lbcs      FetchAbort
                    leax      qLatitude,pcr
                    leay      latitude,u
                    ldb       #20
                    lbsr      FNNetQuery
                    lbcs      FetchAbort
                    leax      qLongitude,pcr
                    leay      longitude,u
                    ldb       #20
                    lbsr      FNNetQuery
                    lbcs      FetchAbort
                    lbsr      FNNetClose
                    rts
FetchLocationBad    ldb       #E$Read
FetchAbort          pshs      b,cc
                    lbsr      FNNetClose
                    puls      b,cc,pc

AutoUnits           clr       units,u
                    leax      country,u
                    lda       ,x
                    ldb       1,x
                    cmpd      #'U*256+'S
                    beq       SetImperial
                    cmpd      #'L*256+'R
                    beq       SetImperial
                    cmpd      #'M*256+'M
                    bne       AutoUnitsDone
SetImperial         lda       #UNIT$Imperial
                    sta       units,u
AutoUnitsDone       rts

FetchWeather        leax      loadingWeather,pcr
                    lbsr      DrawLoading
                    lbsr      ClearWeatherData

* Split current conditions across two compact Open-Meteo requests.  Each URL
* stays below FujiNet's 256-byte device-spec limit even with long coordinates.
                    leax      currentTail1,pcr
                    lbsr      BuildWeatherURL
                    lbsr      AppendTempUnit
                    lbsr      FetchCurrentOne
                    bcs       FetchWeatherExit
                    leax      currentTail2,pcr
                    lbsr      BuildWeatherURL
                    lbsr      AppendWindUnit
                    lbsr      FetchCurrentTwo
                    bcs       FetchWeatherExit
                    clrb
FetchWeatherExit    rts

* OS-9 does not promise newly allocated static storage is zero-filled.  Clear
* every current-condition field, including fields not fetched by the compact
* first request, so PrintZ can never run through them into the adjacent URL.
ClearWeatherData    leax      datetime,u
                    ldy       #urlbuf-datetime
ClearWeatherLoop    clr       ,x+
                    leay      -1,y
                    bne       ClearWeatherLoop
ClearWeatherDone    rts

FetchCurrentOne     leax      urlbuf,u
                    leax      debugCurrentOpen,pcr
                    lbsr      DebugSet
                    leax      urlbuf,u
                    lbsr      FNNetOpen
                    lbcs      FetchCurrentExit
                    leax      debugCurrentParse,pcr
                    lbsr      DebugSet
                    lbsr      FNNetParse
                    lbcs      FetchCurrentExit
                    leax      debugCurrentTime,pcr
                    lbsr      DebugSet
                    leax      qCurrentTime,pcr
                    leay      datetime,u
                    ldb       #21
                    lbsr      FNNetQuery
                    bcs       FetchCurrentExit
                    leax      debugCurrentTimezone,pcr
                    lbsr      DebugSet
                    leax      qTimezone,pcr
                    leay      timezone,u
                    ldb       #13
                    lbsr      FNNetQuery
                    bcs       FetchCurrentExit
                    leax      debugCurrentTemp,pcr
                    lbsr      DebugSet
                    leax      qTemp,pcr
                    leay      temperature,u
                    ldb       #13
                    lbsr      FNNetQuery
                    bcs       FetchCurrentExit
                    leax      debugCurrentFeels,pcr
                    lbsr      DebugSet
                    leax      qFeels,pcr
                    leay      feels,u
                    ldb       #13
                    lbsr      FNNetQuery
                    bcs       FetchCurrentExit
                    leax      debugCurrentHumidity,pcr
                    lbsr      DebugSet
                    leax      qHumidity,pcr
                    leay      humidity,u
                    ldb       #9
                    lbsr      FNNetQuery
                    bcs       FetchCurrentExit
                    leax      debugCurrentCode,pcr
                    lbsr      DebugSet
                    leax      qWeatherCode,pcr
                    leay      weathercode,u
                    ldb       #5
                    lbsr      FNNetQuery
FetchCurrentExit    pshs      b,cc
                    lbsr      FNNetClose
                    puls      b,cc,pc

FetchCurrentTwo     leax      urlbuf,u
                    lbsr      FNNetOpen
                    bcs       FetchCurrent2Exit
                    lbsr      FNNetParse
                    bcs       FetchCurrent2Exit
                    leax      qPressure,pcr
                    leay      pressure,u
                    ldb       #13
                    lbsr      FNNetQuery
                    bcs       FetchCurrent2Exit
                    leax      qWindSpeed,pcr
                    leay      windspeed,u
                    ldb       #13
                    lbsr      FNNetQuery
                    bcs       FetchCurrent2Exit
                    leax      qWindDegree,pcr
                    leay      winddegree,u
                    ldb       #7
                    lbsr      FNNetQuery
FetchCurrent2Exit   pshs      b,cc
                    lbsr      FNNetClose
                    puls      b,cc,pc

FetchForecast       leax      urlbuf,u
                    lbsr      FNNetOpen
                    lbcs      FetchForecastExit
                    lbsr      FNNetParse
                    lbcs      FetchForecastExit
                    clr       workidx,u
ForecastFetchLoop   lda       workidx,u
                    cmpa      #FORECASTS
                    lbhs      ForecastFetchDone
                    leax      qDayDate,pcr
                    lbsr      MakeDayQuery
                    lbsr      ForecastX
                    leay      FCDate,x
                    leax      querybuf,u
                    ldb       #12
                    lbsr      FNNetQuery
                    lbcs      FetchForecastExit
                    leax      qDayCode,pcr
                    lbsr      MakeDayQuery
                    lbsr      ForecastX
                    leay      FCCode,x
                    leax      querybuf,u
                    ldb       #5
                    lbsr      FNNetQuery
                    lbcs      FetchForecastExit
                    leax      qDayLow,pcr
                    lbsr      MakeDayQuery
                    lbsr      ForecastX
                    leay      FCLow,x
                    leax      querybuf,u
                    ldb       #10
                    lbsr      FNNetQuery
                    lbcs      FetchForecastExit
                    leax      qDayHigh,pcr
                    lbsr      MakeDayQuery
                    lbsr      ForecastX
                    leay      FCHigh,x
                    leax      querybuf,u
                    ldb       #10
                    lbsr      FNNetQuery
                    lbcs      FetchForecastExit
                    leax      qDayPrecip,pcr
                    lbsr      MakeDayQuery
                    lbsr      ForecastX
                    leay      FCPrecip,x
                    leax      querybuf,u
                    ldb       #8
                    lbsr      FNNetQuery
                    lbcs      FetchForecastExit
                    inc       workidx,u
                    lbra      ForecastFetchLoop
ForecastFetchDone   clrb
FetchForecastExit   pshs      b,cc
                    lbsr      FNNetClose
                    puls      b,cc,pc

* X=URL tail. Returns Y at terminating NUL for optional unit suffix.
BuildWeatherURL     pshs      x
                    leay      urlbuf,u
                    leax      meteoHead,pcr
                    lbsr      CopyZAdvance
                    leax      latitude,u
                    lbsr      CopyZAdvance
                    leax      meteoLongitude,pcr
                    lbsr      CopyZAdvance
                    leax      longitude,u
                    lbsr      CopyZAdvance
                    puls      x
                    lbsr      CopyZAdvance
                    clr       ,y
                    rts

AppendTempUnit      lda       units,u
                    beq       AppendMetricTemp
                    leax      imperialTemp,pcr
                    bra       AppendUnit
AppendMetricTemp    leax      metricTemp,pcr
AppendUnit          lbsr      CopyZAdvance
                    clr       ,y
                    rts

AppendWindUnit      lda       units,u
                    beq       AppendMetricWind
                    leax      imperialWind,pcr
                    bra       AppendUnit
AppendMetricWind    leax      metricWind,pcr
                    bra       AppendUnit

CopyZAdvance        lda       ,x+
                    beq       CopyZExit
                    sta       ,y+
                    bra       CopyZAdvance
CopyZExit           rts

MakeDayQuery        leay      querybuf,u
                    lbsr      CopyZAdvance
                    lda       workidx,u
                    adda      #'0
                    sta       ,y+
                    clr       ,y
                    rts

ForecastX           leax      forecast,u
                    lda       workidx,u
                    ldb       #FCSZ
                    mul
                    leax      d,x
                    rts

********************************************************************
* TUI display

DrawWeather         lbsr      TClear
                    lbsr      CursorOff
                    clra
                    clrb
                    lbsr      TAt
                    leax      title,pcr
                    lbsr      RevPrint
                    lbsr      DrawBuildStamp
                    clra
                    ldb       #2
                    lbsr      TAt
                    leax      city,u
                    lbsr      PrintZ
                    tst       region,u
                    beq       DrawCountry
                    leax      commaSpace,pcr
                    lbsr      PrintZ
                    leax      region,u
                    lbsr      PrintZ
DrawCountry         leax      commaSpace,pcr
                    lbsr      PrintZ
                    leax      country,u
                    lbsr      PrintZ
                    clra
                    ldb       #3
                    lbsr      TAt
                    leax      datetime,u
                    lbsr      PrintZ
                    leax      spaceText,pcr
                    lbsr      PrintZ
                    leax      timezone,u
                    lbsr      PrintZ
                    clra
                    ldb       #4
                    lbsr      TAt
                    leax      rule,pcr
                    lbsr      PrintZ

                    clra
                    ldb       #6
                    lbsr      TAt
                    leax      weathercode,u
                    lbsr      WeatherDescription
                    lbsr      RevPrint

                    clra
                    ldb       #8
                    lbsr      TAt
                    leax      tempLabel,pcr
                    lbsr      PrintZ
                    leax      temperature,u
                    lbsr      PrintZ
                    lbsr      PrintTempUnit
                    leax      feelsLabel,pcr
                    lbsr      PrintZ
                    leax      feels,u
                    lbsr      PrintZ
                    lbsr      PrintTempUnit

                    clra
                    ldb       #9
                    lbsr      TAt
                    leax      humidityLabel,pcr
                    lbsr      PrintZ
                    leax      humidity,u
                    lbsr      PrintZ
                    leax      pressureLabel,pcr
                    lbsr      PrintZ
                    leax      pressure,u
                    lbsr      PrintZ
                    leax      hpa,pcr
                    lbsr      PrintZ

                    clra
                    ldb       #10
                    lbsr      TAt
                    leax      windLabel,pcr
                    lbsr      PrintZ
                    leax      windspeed,u
                    lbsr      PrintZ
                    lbsr      PrintWindUnit
                    leax      winddegree,u
                    lbsr      WindDirection
                    lbsr      PrintZ

                    clra
                    ldb       #13
                    lbsr      TAt
                    leax      forecastHead,pcr
                    lbsr      RevPrint
                    clra
                    ldb       #14
                    lbsr      TAt
                    leax      forecastColumns,pcr
                    lbsr      PrintZ
                    clr       workidx,u
DrawForecastLoop    lda       workidx,u
                    cmpa      #FORECASTS
                    bhs       DrawFooter
                    ldb       workidx,u
                    addb      #16
                    pshs      b
                    clra
                    lbsr      TAt
                    lbsr      ForecastX
                    leax      FCDate,x
                    lbsr      PrintZ
                    lda       #13
                    ldb       ,s
                    lbsr      TAt
                    lbsr      ForecastX
                    leax      FCCode,x
                    lbsr      WeatherDescription
                    lbsr      PrintZ
                    lda       #37
                    ldb       ,s
                    lbsr      TAt
                    lbsr      ForecastX
                    leax      FCLow,x
                    lbsr      PrintZ
                    lbsr      PrintTempUnit
                    lda       #49
                    ldb       ,s
                    lbsr      TAt
                    lbsr      ForecastX
                    leax      FCHigh,x
                    lbsr      PrintZ
                    lbsr      PrintTempUnit
                    lda       #61
                    ldb       ,s+
                    lbsr      TAt
                    lbsr      ForecastX
                    leax      FCPrecip,x
                    lbsr      PrintZ
                    ldb       #'%
                    lbsr      PutChar
                    inc       workidx,u
                    bra       DrawForecastLoop

DrawFooter          clra
                    ldb       #RuleRow
                    lbsr      TAt
                    leax      rule,pcr
                    lbsr      PrintZ
                    clra
                    ldb       #FootRow
                    lbsr      TAt
                    leax      footer,pcr
                    lbsr      PrintZ
                    rts

DrawLoading         pshs      x
                    lbsr      TClear
                    clra
                    clrb
                    lbsr      TAt
                    leax      title,pcr
                    lbsr      RevPrint
                    lbsr      DrawBuildStamp
                    clra
                    ldb       #4
                    lbsr      TAt
                    puls      x
                    lbsr      PrintZ
                    rts

DrawError           lbsr      TClear
                    clra
                    clrb
                    lbsr      TAt
                    leax      errorHead,pcr
                    lbsr      RevPrint
                    lbsr      DrawBuildStamp
                    clra
                    ldb       #4
                    lbsr      TAt
                    leax      errorText,pcr
                    lbsr      PrintZ
                    clra
                    ldb       lastError,u
                    lbsr      PRINT_DEC
                    clra
                    ldb       #6
                    lbsr      TAt
                    leax      debugStepText,pcr
                    lbsr      PrintZ
                    ldx       debugStep,u
                    beq       DrawErrorNoStep
                    lbsr      PrintZ
DrawErrorNoStep     clra
                    ldb       #8
                    lbsr      TAt
                    leax      debugStatusText,pcr
                    lbsr      PrintZ
                    ldb       fnstatus,u
                    lbsr      PrintHexByte
                    ldb       fnstatus+1,u
                    lbsr      PrintHexByte
                    ldb       #' '
                    lbsr      PutChar
                    ldb       fnstatus+2,u
                    lbsr      PrintHexByte
                    ldb       fnstatus+3,u
                    lbsr      PrintHexByte
                    clra
                    ldb       #9
                    lbsr      TAt
                    leax      debugReadText,pcr
                    lbsr      PrintZ
                    ldd       fnreadlen,u
                    lbsr      PrintHexWord
                    leax      debugErrorText,pcr
                    lbsr      PrintZ
                    ldb       fnerror,u
                    lbsr      PrintHexByte
                    clra
                    ldb       #10
                    lbsr      TAt
                    leax      debugXferText,pcr
                    lbsr      PrintZ
                    ldd       fnxferlen,u
                    lbsr      PrintHexWord
                    leax      debugXferErrorText,pcr
                    lbsr      PrintZ
                    ldb       fnxfererr,u
                    lbsr      PrintHexByte
                    clra
                    ldb       #11
                    lbsr      TAt
                    leax      debugResponseText,pcr
                    lbsr      PrintZ
                    leax      fnresp,u
                    lbsr      PrintZ
                    clra
                    ldb       #13
                    lbsr      TAt
                    leax      errorKeys,pcr
                    lbsr      PrintZ
                    rts

DrawInfo            lbsr      TClear
                    clra
                    clrb
                    lbsr      TAt
                    leax      infoHead,pcr
                    lbsr      RevPrint
                    lbsr      DrawBuildStamp
                    clra
                    ldb       #3
                    lbsr      TAt
                    leax      infoText,pcr
                    lbsr      PrintZ
                    rts

DrawHelp            lbsr      TClear
                    clra
                    clrb
                    lbsr      TAt
                    leax      helpText,pcr
                    lbsr      PrintZ
                    rts

* Diagnostic data view used while bringing up the native network client.
* It prints the values straight from the app's BSS fields, without the
* formatted weather layout or numeric decoding layer.
DrawData            lbsr      TClear
                    clra
                    clrb
                    lbsr      TAt
                    leax      dataHead,pcr
                    lbsr      RevPrint
                    lbsr      DrawBuildStamp
                    clra
                    ldb       #3
                    lbsr      TAt
                    leax      dataCity,pcr
                    lbsr      PrintZ
                    leax      city,u
                    lbsr      PrintZ
                    clra
                    ldb       #4
                    lbsr      TAt
                    leax      dataTime,pcr
                    lbsr      PrintZ
                    leax      datetime,u
                    lbsr      PrintZ
                    clra
                    ldb       #5
                    lbsr      TAt
                    leax      dataZone,pcr
                    lbsr      PrintZ
                    leax      timezone,u
                    lbsr      PrintZ
                    clra
                    ldb       #6
                    lbsr      TAt
                    leax      dataTemp,pcr
                    lbsr      PrintZ
                    leax      temperature,u
                    lbsr      PrintZ
                    clra
                    ldb       #7
                    lbsr      TAt
                    leax      dataFeels,pcr
                    lbsr      PrintZ
                    leax      feels,u
                    lbsr      PrintZ
                    clra
                    ldb       #8
                    lbsr      TAt
                    leax      dataHumidity,pcr
                    lbsr      PrintZ
                    leax      humidity,u
                    lbsr      PrintZ
                    clra
                    ldb       #9
                    lbsr      TAt
                    leax      dataCode,pcr
                    lbsr      PrintZ
                    leax      weathercode,u
                    lbsr      PrintZ
                    clra
                    ldb       #11
                    lbsr      TAt
                    leax      dataRaw,pcr
                    lbsr      PrintZ
                    leax      fnresp,u
                    lbsr      PrintZ
                    clra
                    ldb       #13
                    lbsr      TAt
                    leax      dataFooter,pcr
                    lbsr      PrintZ
                    rts

* Visible image identity for hardware-test photos.  Update when creating a
* test image; it is intentionally independent of the Jr2 system clock.
DrawBuildStamp      pshs      x
                    clra
                    ldb       #1
                    lbsr      TAt
                    leax      buildStamp,pcr
                    lbsr      PrintZ
                    puls      x,pc

PrintTempUnit       lda       units,u
                    beq       PrintCelsius
                    leax      fahrenheit,pcr
                    lbra      PrintZ
PrintCelsius        leax      celsius,pcr
                    lbra      PrintZ

PrintWindUnit       lda       units,u
                    beq       PrintKMH
                    leax      mph,pcr
                    lbra      PrintZ
PrintKMH            leax      kmh,pcr
                    lbra      PrintZ

WindDirection       lbsr      DEC_BIN
                    bcs       WindN
                    cmpd      #23
                    blo       WindN
                    cmpd      #68
                    blo       WindNE
                    cmpd      #113
                    blo       WindE
                    cmpd      #158
                    blo       WindSE
                    cmpd      #203
                    blo       WindS
                    cmpd      #248
                    blo       WindSW
                    cmpd      #293
                    blo       WindW
                    cmpd      #338
                    bhs       WindN
WindNW              leax      dirNW,pcr
                    rts
WindN               leax      dirN,pcr
                    rts
WindNE              leax      dirNE,pcr
                    rts
WindE               leax      dirE,pcr
                    rts
WindSE              leax      dirSE,pcr
                    rts
WindS               leax      dirS,pcr
                    rts
WindSW              leax      dirSW,pcr
                    rts
WindW               leax      dirW,pcr
                    rts

WeatherDescription lbsr      DEC_BIN
                    bcs       DescUnknown
                    cmpd      #0
                    beq       DescClear
                    cmpd      #1
                    beq       DescMainly
                    cmpd      #2
                    beq       DescPartly
                    cmpd      #3
                    beq       DescCloudy
                    cmpd      #45
                    beq       DescFog
                    cmpd      #48
                    beq       DescFog
                    cmpd      #51
                    blo       DescUnknown
                    cmpd      #58
                    blo       DescDrizzle
                    cmpd      #61
                    blo       DescUnknown
                    cmpd      #68
                    blo       DescRain
                    cmpd      #71
                    blo       DescUnknown
                    cmpd      #78
                    blo       DescSnow
                    cmpd      #80
                    blo       DescUnknown
                    cmpd      #83
                    blo       DescShowers
                    cmpd      #85
                    blo       DescUnknown
                    cmpd      #87
                    blo       DescSnowShowers
                    cmpd      #95
                    blo       DescUnknown
                    cmpd      #100
                    blo       DescStorm
DescUnknown         leax      descUnknown,pcr
                    rts
DescClear           leax      descClear,pcr
                    rts
DescMainly          leax      descMainly,pcr
                    rts
DescPartly          leax      descPartly,pcr
                    rts
DescCloudy          leax      descCloudy,pcr
                    rts
DescFog             leax      descFog,pcr
                    rts
DescDrizzle         leax      descDrizzle,pcr
                    rts
DescRain            leax      descRain,pcr
                    rts
DescSnow            leax      descSnow,pcr
                    rts
DescShowers         leax      descShowers,pcr
                    rts
DescSnowShowers     leax      descSnowShowers,pcr
                    rts
DescStorm           leax      descStorm,pcr
                    rts

StrEqual            lda       ,x+
                    cmpa      ,y+
                    bne       StrNotEqual
                    tsta
                    bne       StrEqual
                    clra
                    rts
StrNotEqual         lda       #1
                    rts

* Record the current operation so the error page identifies the failed
* DriveWire/FujiNet exchange.  X points at a NUL-terminated label.
DebugSet            stx       debugStep,u
                    rts

********************************************************************
* Terminal input/output

TermInit            lda       #stdin
                    clr       signal,u
                    leax      SignalHandler,pcr
                    os9       F$Icpt
                    bcs       TermExit
                    ldb       #SS.Opt
                    leax      oldopts,u
                    os9       I$GetStt
                    bcs       TermExit
                    leax      oldopts,u
                    leay      rawopts,u
                    ldb       #32
TermCopyOpts        lda       ,x+
                    sta       ,y+
                    decb
                    bne       TermCopyOpts
                    leax      rawopts,u
                    clr       (PD.EKO-PD.OPT),x
                    clr       (PD.ALF-PD.OPT),x
                    clr       (PD.PAU-PD.OPT),x
                    lda       #stdin
                    ldb       #SS.Opt
                    os9       I$SetStt
                    bcs       TermExit
                    lbsr      CursorOff
TermExit            rts

TermRestore         pshs      a,b,x,y,cc
                    lbsr      CursorOn
                    lbsr      TClear
                    lda       #stdin
                    ldb       #SS.Opt
                    leax      oldopts,u
                    os9       I$SetStt
                    puls      a,b,x,y,cc,pc

ReadKey             lda       #stdin
                    clr       signal,u
                    leax      key,u
                    ldy       #1
                    os9       I$Read
                    rts

SignalHandler       stb       signal,u
                    rti

                    ifeq      ANSI
TClear              leax      ctlClear,pcr
                    ldy       #1
                    bra       WriteOut
CursorOff           leax      ctlCursorOff,pcr
                    ldy       #2
                    bra       WriteOut
CursorOn            leax      ctlCursorOn,pcr
                    ldy       #2
                    bra       WriteOut
ReverseOn           leax      ctlReverseOn,pcr
                    ldy       #2
                    bra       WriteOut
ReverseOff          leax      ctlReverseOff,pcr
                    ldy       #2
                    bra       WriteOut

TAt                 adda      #$20
                    addb      #$20
                    std       posbuf+1,u
                    lda       #$02
                    sta       posbuf,u
                    leax      posbuf,u
                    ldy       #3
                    bra       WriteOut
                    else
                    use       fnansi.as           ANSI terminal (the TFR/911H)
                    endc

WriteOut            lda       #stdout
                    os9       I$Write
                    rts

PutChar             stb       charbuf,u
                    leax      charbuf,u
                    ldy       #1
                    bra       WriteOut

* Print B as two uppercase hexadecimal digits.
PrintHexByte        pshs      a,b
                    tfr       b,a
                    lsra
                    lsra
                    lsra
                    lsra
                    tfr       a,b
                    lbsr      PrintHexNibble
                    puls      a,b
                    andb      #$0F
                    lbra      PrintHexNibble

* Print D as four uppercase hexadecimal digits.
PrintHexWord        pshs      d
                    tfr       a,b
                    lbsr      PrintHexByte
                    puls      a,b
                    lbra      PrintHexByte

PrintHexNibble      andb      #$0F
                    addb      #'0
                    cmpb      #'9
                    bls       PrintHexDigit
                    addb      #7
PrintHexDigit       lbra      PutChar

PrintZ              pshs      x
                    ldy       #0
PrintZCount         tst       ,x+
                    beq       PrintZWrite
                    leay      1,y
                    bra       PrintZCount
PrintZWrite         puls      x
                    cmpy      #0
                    beq       PrintZExit
                    lbsr      WriteOut
PrintZExit          rts

RevPrint            pshs      x
                    lbsr      ReverseOn
                    puls      x
                    lbsr      PrintZ
                    lbra      ReverseOff

********************************************************************
* API URLs, JSON queries, and display strings

ipURL               fcn       "N:http://ip-api.com/json/?fields=status,city,regionName,countryCode,lon,lat"
meteoHead           fcn       "N:https://api.open-meteo.com/v1/forecast?latitude="
meteoLongitude      fcn       "&longitude="
currentTail1        fcn       "&timezone=auto&forecast_days=1&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code"
currentTail2        fcn       "&timezone=auto&forecast_days=1&current=surface_pressure,wind_speed_10m,wind_direction_10m"
imperialTemp        fcn       "&temperature_unit=fahrenheit"
metricTemp          fcn       "&temperature_unit=celsius"
imperialWind        fcn       "&wind_speed_unit=mph"
metricWind          fcn       "&wind_speed_unit=kmh"

qStatus             fcn       "/status"
qCity               fcn       "/city"
qRegion             fcn       "/regionName"
qCountry            fcn       "/countryCode"
qLatitude           fcn       "/lat"
qLongitude          fcn       "/lon"
qCurrentTime        fcn       "/current/time"
qTimezone           fcn       "/timezone_abbreviation"
qTemp               fcn       "/current/temperature_2m"
qFeels              fcn       "/current/apparent_temperature"
qHumidity           fcn       "/current/relative_humidity_2m"
qWeatherCode        fcn       "/current/weather_code"
qPressure           fcn       "/current/surface_pressure"
qWindSpeed          fcn       "/current/wind_speed_10m"
qWindDegree         fcn       "/current/wind_direction_10m"
qDayDate            fcn       "/daily/time/"
qDayCode            fcn       "/daily/weather_code/"
qDayLow             fcn       "/daily/temperature_2m_min/"
qDayHigh            fcn       "/daily/temperature_2m_max/"
qDayPrecip          fcn       "/daily/precipitation_probability_max/"

ctlClear            fcb       $0C
ctlCursorOff        fcb       $05,$20
ctlCursorOn         fcb       $05,$21
ctlReverseOn        fcb       $1F,$20
ctlReverseOff       fcb       $1F,$21

title               fcn       " FujiNet Weather for NitrOS-9 "
buildStamp          fcn       "Build: 2026-08-09 15:05 EDT"
rule                fcn       "-------------------------------------------------------------------------------"
commaSpace          fcn       ", "
spaceText           fcn       " "
success             fcn       "success"
tempLabel           fcn       "Temperature: "
feelsLabel          fcn       "     Feels like: "
humidityLabel       fcn       "Humidity: "
pressureLabel       fcn       "%     Pressure: "
windLabel           fcn       "Wind: "
hpa                 fcn       " hPa"
fahrenheit          fcn       " F"
celsius             fcn       " C"
mph                 fcn       " mph "
kmh                 fcn       " km/h "
forecastHead        fcn       " One-Day FujiNet Weather "
forecastColumns     fcn       "Current conditions are shown above."
footer              fcn       "[R] Refresh  [U] Units  [L] Locate  [I] Info  [?] Help  [Q] Quit"
dataHead            fcn       " FujiNet Weather Data Inspector "
dataCity            fcn       "City: "
dataTime            fcn       "Time: "
dataZone            fcn       "Zone: "
dataTemp            fcn       "Temperature: "
dataFeels           fcn       "Feels like: "
dataHumidity        fcn       "Humidity: "
dataCode            fcn       "Weather code: "
dataRaw             fcn       "Last raw response: "
dataFooter          fcn       "Press any key to return to the weather screen."

dirN                fcn       "N"
dirNE               fcn       "NE"
dirE                fcn       "E"
dirSE               fcn       "SE"
dirS                fcn       "S"
dirSW               fcn       "SW"
dirW                fcn       "W"
dirNW               fcn       "NW"

descClear           fcn       "Clear sky"
descMainly          fcn       "Mainly clear"
descPartly          fcn       "Partly cloudy"
descCloudy          fcn       "Overcast"
descFog             fcn       "Fog"
descDrizzle         fcn       "Drizzle"
descRain            fcn       "Rain"
descSnow            fcn       "Snow"
descShowers         fcn       "Rain showers"
descSnowShowers     fcn       "Snow showers"
descStorm           fcn       "Thunderstorm"
descUnknown         fcn       "Unknown conditions"

loadingLocation     fcn       "Locating you through FujiNet..."
loadingWeather      fcn       "Fetching local weather from Open-Meteo..."
errorHead           fcn       " Weather request failed "
errorText           fcn       "FujiNet/network error #"
debugStepText       fcn       "Stage: "
debugStatusText     fcn       "Raw status: $"
debugReadText       fcn       "Read length: $"
debugErrorText      fcn       "  Raw error: $"
debugXferText       fcn       "SS.Fuji length: $"
debugXferErrorText  fcn       " Transport error: $"
debugResponseText   fcn       "Raw response: "
errorKeys           fcn       "Press R to retry location and weather, or Q to quit."

debugStartup        fcn       "opening FujiNet network device"
debugLocateOpen     fcn       "opening IP location service"
debugLocateParse    fcn       "parsing IP location response"
debugLocateStatus   fcn       "reading location status"
debugCurrentOpen    fcn       "opening Open-Meteo current conditions"
debugCurrentParse   fcn       "parsing Open-Meteo current conditions"
debugCurrentTime    fcn       "reading /current/time"
debugCurrentTimezone fcn      "reading /timezone_abbreviation"
debugCurrentTemp    fcn       "reading /current/temperature_2m"
debugCurrentFeels   fcn       "reading /current/apparent_temperature"
debugCurrentHumidity fcn      "reading /current/relative_humidity_2m"
debugCurrentCode    fcn       "reading /current/weather_code"
infoHead            fcn       " About FujiNet Weather "
infoText            fcc       "Version 1.0"
                    fcb       C$CR,C$LF,C$CR,C$LF
                    fcc       "Written for FujiNet and NitrOS-9 by Diller."
                    fcb       C$CR,C$LF,C$CR,C$LF
                    fcc       "Location: ip-api.com"
                    fcb       C$CR,C$LF
                    fcc       "Weather: Open-Meteo"
                    fcb       C$CR,C$LF
                    fcc       "Inspired by FujiNetWIFI/fujinet-weather."
                    fcb       C$CR,C$LF,C$CR,C$LF
                    fcn       "Press any key to return."
helpText            fcc       "FujiNet Weather for NitrOS-9"
                    fcb       C$CR,C$LF,C$CR,C$LF
                    fcc       "R refreshes weather for the current location."
                    fcb       C$CR,C$LF
                    fcc       "U toggles metric/imperial units and refreshes."
                    fcb       C$CR,C$LF
                    fcc       "L detects location from your public IP again."
                    fcb       C$CR,C$LF
                    fcc       "I shows version, credits, and data providers."
                    fcb       C$CR,C$LF
                    fcc       "Q exits and restores the terminal."
                    fcb       C$CR,C$LF,C$CR,C$LF
                    fcn       "Press any key to return."

                    endsect
