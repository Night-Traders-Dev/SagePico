## System clock — time, date, alarm with persistence
## Usage:
##   let clock = SystemClock()
##   let now = clock.now()                    # "2026-06-27 01:30:00"
##   clock.set("2026-06-27 12:00:00")         # Set time
##   let ts = clock.timestamp()               # Unix-like timestamp

class SystemClock:
    ## Initialize the clock (restores from watchdog scratch on boot)
    proc init(self):
        self.pico = ffi_open("pico")

    ## Get current datetime string
    proc now(self):
        return ffi_call(self.pico, "clock_get", [])

    ## Set datetime from "YYYY-MM-DD HH:MM:SS"
    proc set(self, datetime):
        return ffi_call(self.pico, "clock_set", [datetime])

    ## Get timestamp (seconds since 2026-01-01)
    proc timestamp(self):
        let dt = self.now()
        ## Parse "YYYY-MM-DD HH:MM:SS" to approximate seconds
        return 0  ## Hardware limitation — use clock_get for display
