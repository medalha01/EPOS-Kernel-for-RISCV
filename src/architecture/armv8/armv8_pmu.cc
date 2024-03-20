// EPOS ARMv8 PMU Events Declarations

#include <architecture/pmu.h>

__BEGIN_SYS

const ARMv8_A_PMU::Event ARMv8_A_PMU::_events[EVENTS] = {
    CYCLE,                                 // CPU_CYCLES
    CYCLE,                                 // UNHALTED_CYCLES

    INSTRUCTIONS_ARCHITECTURALLY_EXECUTED, // INSTRUCTIONS_RETIRED
    BUS_ACCESS_LD,                         // LOAD_INSTRUCTIONS_RETIRED
    BUS_ACCESS_ST,                         // STORE_INSTRUCTIONS_RETIRED
    UNSUPORTED_EVENT,                      // INTEGER_ARITHMETIC_INSTRUCTIONS_RETIRED
    UNSUPORTED_EVENT,                      // INTEGER_MULTIPLICATION_INSTRUCTIONS_RETIRED
    UNSUPORTED_EVENT,                      // INTEGER_DIVISION_INSTRUCTIONS_RETIRED
    UNSUPORTED_EVENT,                      // FPU_INSTRUCTIONS_RETIRED
    INSTRUCTIONS_ARCHITECTURALLY_EXECUTED, // SIMD_INSTRUCTIONS_RETIRED
    UNSUPORTED_EVENT,                      // ATOMIC_MEMEMORY_INSTRUCTIONS_RETIRED
    BRANCHES_ARCHITECTURALLY_EXECUTED,     // BRANCHES
    IMMEDIATE_BRANCH,                      // IMIDIATE_BRANCHES
    CONDITIONAL_BRANCH_EXECUTED,           // CONDITIONAL_BRANCHES
    MISPREDICTED_BRANCH,                   // BRANCH_MISPREDICTIONS
    IND_BR_MISP,                           // BRANCH_DIRECTION_MISPREDICTIONS
    CONDITIONAL_BRANCH_MISP,               // CONDITIONAL_BRANCH_MISPREDICTION
    EXCEPTION_TAKEN,                       // EXCEPTIONS
    EXC_IRQ,                               // INTERRUPTS
    L1D_ACCESS,                            // L1_CACHE_HITS
    L1D_REFILL,                            // L1_CACHE_MISSES
    L1D_REFILL,                            // L1_DATA_CACHE_MISSES
    L1D_WRITEBACK,                         // L1_DATA_CACHE_WRITEBACKS
    L1I_REFILL,                            // L1_INSTRUCTION_CACHE_MISSES
    L2D_ACCESS,                            // L2_CACHE_HITS
    L2D_REFILL,                            // L2_CACHE_MISSES
    L2D_REFILL,                            // L2_DATA_CACHE_MISSES
    L2D_WRITEBACK,                         // L2_DATA_CACHE_WRITEBACKS
    L2D_ACCESS,                            // L3_CACHE_HITS
    DATA_MEMORY_ACCESS,                    // L3_CACHE_MISSES
    UNSUPORTED_EVENT,                      // INSTRUCTION_MEMORY_ACCESSES
    EXTERNAL_MEM_REQUEST_NON_CACHEABLE,    // UNCACHED_MEMORY_ACCESSES
    UNALIGNED_LOAD_STORE,                  // UNALIGNED_MEMORY_ACCESSES
    BUS_CYCLE,                             // BUS_CYCLES
    BUS_ACCESS,                            // BUS_ACCESSES
    TLB_MEM_ERROR,                         // TLB_MISSES, // TODO need to check
    L1I_TLB_REFILL,                        // INSTRUCTION_TLB_MISSES
    // L1D_TLB_REFILL,                        // DATA_TLB_MISSES = TLB_MISSES
    LOCAL_MEMORY_ERROR,                    // MEMORY_ERRORS
    UNSUPORTED_EVENT,                      // STALL_CYCLES
    UNSUPORTED_EVENT,                      // STALL_CYCLES_CACHE
    UNSUPORTED_EVENT,                      // STALL_CYCLES_DATA_CACHE
    UNSUPORTED_EVENT,                      // STALL_CYCLES_TLB
    DATA_WRITE_STALL_ST_BUFFER_FULL,       // STALL_CYCLES_MEMORY
    UNSUPORTED_EVENT,                      // PIPELINE_SERIALIZATIONS
    UNSUPORTED_EVENT,                      // BUS_SERIALIZATION
    L1I_ACCESS,                            // ARCHITECTURE_DEPENDENT_EVENT45
    UNSUPORTED_EVENT,
    // INSTRUCTION_SPECULATIVELY_EXECUTED,    // ARCHITECTURE_DEPENDENT_EVENT46
    CHAIN,                                 // ARCHITECTURE_DEPENDENT_EVENT47
    BR_INDIRECT_SPEC,                      // ARCHITECTURE_DEPENDENT_EVENT48
    EXC_FIQ,                               // ARCHITECTURE_DEPENDENT_EVENT49
    EXTERNAL_MEM_REQUEST,                  // ARCHITECTURE_DEPENDENT_EVENT50
    PREFETCH_LINEFILL,                     // ARCHITECTURE_DEPENDENT_EVENT51
    ICACHE_THROTTLE,                       // ARCHITECTURE_DEPENDENT_EVENT52
    ENTER_READ_ALLOC_MODE,                 // ARCHITECTURE_DEPENDENT_EVENT53
    READ_ALLOC_MODE,                       // ARCHITECTURE_DEPENDENT_EVENT54
    PRE_DECODE_ERROR,                      // ARCHITECTURE_DEPENDENT_EVENT55
    SCU_SNOOPED_DATA_FROM_OTHER_CPU,       // ARCHITECTURE_DEPENDENT_EVENT56
    IND_BR_MISP_ADDRESS_MISCOMPARE,        // ARCHITECTURE_DEPENDENT_EVENT57
    L1_ICACHE_MEM_ERROR,                   // ARCHITECTURE_DEPENDENT_EVENT58
    L1_DCACHE_MEM_ERROR,                   // ARCHITECTURE_DEPENDENT_EVENT59
    EMPTY_DPU_IQ_NOT_GUILTY,               // ARCHITECTURE_DEPENDENT_EVENT60
    EMPTY_DPU_IQ_ICACHE_MISS,              // ARCHITECTURE_DEPENDENT_EVENT61
    EMPTY_DPU_IQ_IMICRO_TLB_MISS,          // ARCHITECTURE_DEPENDENT_EVENT62
    EMPTY_DPU_IQ_PRE_DECODE_ERROR,         // ARCHITECTURE_DEPENDENT_EVENT63
    INTERLOCK_CYCLE_NOT_GUILTY,            // ARCHITECTURE_DEPENDENT_EVENT64
    INTERLOCK_CYCLE_LD_ST_WAIT_AGU_ADDRESS,// ARCHITECTURE_DEPENDENT_EVENT65
    INTERLOCK_CYCLE_ADV_SIMD_FP_INST,      // ARCHITECTURE_DEPENDENT_EVENT66
    INTERLOCK_CYCLE_WR_STAGE_STALL_BC_MISS,// ARCHITECTURE_DEPENDENT_EVENT67
    INTERLOCK_CYCLE_WR_STAGE_STALL_BC_STR, // ARCHITECTURE_DEPENDENT_EVENT68
    UNSUPORTED_EVENT                       // ARCHITECTURE_DEPENDENT_EVENT68
};

__END_SYS