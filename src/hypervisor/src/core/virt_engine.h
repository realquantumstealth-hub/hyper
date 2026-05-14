#pragma once
#include <cstdint>
#include "definitions.h"

// Abstract base class for virtualization engines
class VirtualizationEngine
{
public:
    enum class VendorType
    {
        Intel,
        AMD,
        Unknown
    };

    virtual ~VirtualizationEngine() = default;

    // Pure virtual methods that must be implemented by vendor-specific classes
    virtual bool initialize() = 0;
    virtual void cleanup() = 0;
    
    virtual std::uint64_t get_exit_reason() = 0;
    virtual void* get_guest_context() = 0;
    virtual void* get_vmcs_vmcb() = 0;
    
    virtual bool handle_ept_violation(std::uint64_t gpa, std::uint64_t gva) = 0;
    virtual bool handle_cpuid(std::uint32_t leaf, std::uint32_t subleaf) = 0;
    
    virtual void invalidate_ept_cache() = 0;
    virtual void flush_tlb() = 0;
    
    virtual std::uint64_t read_guest_register(std::uint32_t reg) = 0;
    virtual void write_guest_register(std::uint32_t reg, std::uint64_t value) = 0;
    
    virtual std::uint64_t get_guest_cr3() = 0;
    virtual std::uint64_t get_ept_pointer() = 0;
    
    // Stack manipulation for anti-detection
    virtual void* setup_shadow_stack() = 0;
    virtual void switch_to_shadow_stack(void* shadow_stack) = 0;
    virtual void restore_original_stack() = 0;
    
    // PTE manipulation
    virtual void hide_page_from_pte(std::uint64_t virtual_address) = 0;
    virtual void restore_page_in_pte(std::uint64_t virtual_address) = 0;
    
    static VendorType detect_vendor();
    static std::unique_ptr<VirtualizationEngine> create();
    
protected:
    VendorType vendor_type_;
    void* shadow_stack_base_ = nullptr;
    void* original_stack_ = nullptr;
};

// Intel VMX implementation
class IntelVMX : public VirtualizationEngine
{
public:
    IntelVMX() { vendor_type_ = VendorType::Intel; }
    
    bool initialize() override;
    void cleanup() override;
    
    std::uint64_t get_exit_reason() override;
    void* get_guest_context() override;
    void* get_vmcs_vmcb() override;
    
    bool handle_ept_violation(std::uint64_t gpa, std::uint64_t gva) override;
    bool handle_cpuid(std::uint32_t leaf, std::uint32_t subleaf) override;
    
    void invalidate_ept_cache() override;
    void flush_tlb() override;
    
    std::uint64_t read_guest_register(std::uint32_t reg) override;
    void write_guest_register(std::uint32_t reg, std::uint64_t value) override;
    
    std::uint64_t get_guest_cr3() override;
    std::uint64_t get_ept_pointer() override;
    
    void* setup_shadow_stack() override;
    void switch_to_shadow_stack(void* shadow_stack) override;
    void restore_original_stack() override;
    
    void hide_page_from_pte(std::uint64_t virtual_address) override;
    void restore_page_in_pte(std::uint64_t virtual_address) override;
    
private:
    void* vmcs_ = nullptr;
};

// AMD SVM implementation
class AmdSVM : public VirtualizationEngine
{
public:
    AmdSVM() { vendor_type_ = VendorType::AMD; }
    
    bool initialize() override;
    void cleanup() override;
    
    std::uint64_t get_exit_reason() override;
    void* get_guest_context() override;
    void* get_vmcs_vmcb() override;
    
    bool handle_ept_violation(std::uint64_t gpa, std::uint64_t gva) override;
    bool handle_cpuid(std::uint32_t leaf, std::uint32_t subleaf) override;
    
    void invalidate_ept_cache() override;
    void flush_tlb() override;
    
    std::uint64_t read_guest_register(std::uint32_t reg) override;
    void write_guest_register(std::uint32_t reg, std::uint64_t value) override;
    
    std::uint64_t get_guest_cr3() override;
    std::uint64_t get_ept_pointer() override;
    
    void* setup_shadow_stack() override;
    void switch_to_shadow_stack(void* shadow_stack) override;
    void restore_original_stack() override;
    
    void hide_page_from_pte(std::uint64_t virtual_address) override;
    void restore_page_in_pte(std::uint64_t virtual_address) override;
    
private:
    void* vmcb_ = nullptr;
    void* host_save_area_ = nullptr;
};
