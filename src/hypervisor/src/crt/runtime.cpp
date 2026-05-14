// Disable all intrinsics for this entire compilation unit
#pragma intrinsic()

// Force the compiler to generate actual function calls
#pragma function(memset)

extern "C" {
    // memset - use volatile to prevent optimization
    void* memset(void* dest, int value, unsigned long long count)
    {
        volatile unsigned char* ptr = (volatile unsigned char*)dest;
        unsigned char val = (unsigned char)value;
        
        for (unsigned long long i = 0; i < count; i++) {
            ptr[i] = val;
        }
        
        return dest;
    }
    
    // __chkstk - stack probe function stub
    void __chkstk(void)
    {
        // Empty implementation for kernel mode
    }
}
