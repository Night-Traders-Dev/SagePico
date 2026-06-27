## SHA256 — hardware-accelerated hashing with batching and verification
## Usage:
##   let sha2 = SHA256()
##   let h = sha2.hash("hello")              # Single hash
##   let results = sha2.hash_batch(["a","b"]) # Batch (sequential DMA)
##   let ok = sha2.verify(data, expected)     # Compare against known hash

class SHA256:
    ## Initialize SHA-256 engine
    proc init(self):
        self.pico = ffi_open("pico")
        self.cache = {}

    ## Hash a single string. Returns 64-char hex digest.
    proc hash(self, data):
        ## Check cache for identical input
        if dict_has(self.cache, data):
            return self.cache[data]
        let result = ffi_call(self.pico, "sha256", [data])
        self.cache[data] = result
        return result

    ## Hash multiple strings in batch
    proc hash_batch(self, items):
        let results = []
        for item in items:
            push(results, self.hash(item))
        return results

    ## Verify data matches expected hash
    proc verify(self, data, expected):
        let actual = self.hash(data)
        return actual == expected

    ## Clear the result cache
    proc clear_cache(self):
        self.cache = {}
