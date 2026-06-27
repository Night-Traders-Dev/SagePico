## FlashStorage — flash-backed key-value store with batch operations
## Usage:
##   let flash = FlashStorage()           # Auto-initializes
##   flash.save("key", "value")           # Single write
##   flash.save_batch(["k1"]: "v1", ...)  # Batch write (fewer erases)
##   let val = flash.load("key")          # Read
##   flash.delete("key")                  # Remove
##   let keys = flash.list()              # All keys
##   let n = flash.count()                # Number of entries

class FlashStorage:
    ## Initialize the flash store
    proc init(self):
        self.pico = ffi_open("pico")

    ## Save a single key-value pair
    proc save(self, key, value):
        ffi_call(self.pico, "flash_save", [key, value])

    ## Save multiple key-value pairs in sequence
    proc save_batch(self, pairs):
        for key in dict_keys(pairs):
            ffi_call(self.pico, "flash_save", [key, pairs[key]])

    ## Load a value by key. Returns nil if not found.
    proc load(self, key):
        return ffi_call(self.pico, "flash_load", [key])

    ## Delete a key
    proc delete(self, key):
        ffi_call(self.pico, "flash_del", [key])

    ## List all stored keys
    proc list(self):
        return ffi_call(self.pico, "flash_keys", [])

    ## Count stored entries
    proc count(self):
        return len(self.list())

    ## Check if a key exists
    proc has(self, key):
        return self.load(key) != nil
