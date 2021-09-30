#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <climits>
#include <thread>
#include <chrono>

#include <cpuid.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <fmt/format.h>

#define INTEL_FAM6_NEHALEM 0x1E
#define INTEL_FAM6_NEHALEM_G 0x1F /* Auburndale / Havendale */
#define INTEL_FAM6_NEHALEM_EP 0x1A
#define INTEL_FAM6_NEHALEM_EX 0x2E

#define INTEL_FAM6_WESTMERE 0x25
#define INTEL_FAM6_WESTMERE_EP 0x2C
#define INTEL_FAM6_WESTMERE_EX 0x2F

#define INTEL_FAM6_SKYLAKE_L 0x4E       /* Sky Lake             */
#define INTEL_FAM6_SKYLAKE 0x5E         /* Sky Lake             */
#define INTEL_FAM6_ATOM_GOLDMONT 0x5C   /* Apollo Lake */
#define INTEL_FAM6_ATOM_GOLDMONT_D 0x5F /* Denverton */

/* Note: the micro-architecture is "Goldmont Plus" */
#define INTEL_FAM6_ATOM_GOLDMONT_PLUS 0x7A /* Gemini Lake */

#define PROCESSOR_NAME_MAXLEN 16

enum class ProcessorVendor
{
    INTEL,
    AMD,
    HYGON,
    UNKNOWN
};

struct ProcessorInfo
{
    ProcessorVendor vendor{ProcessorVendor::UNKNOWN};
    unsigned int maxLevel{0};
    std::string name;
    uint8_t model{0};
    uint8_t family{0};
    uint64_t tscHz{0};
};

static inline uint64_t ReadTsc()
{
    unsigned int lo, hi;

    asm volatile("lfence" ::: "memory");
    asm volatile("rdtsc\n" : "=a"(lo), "=d"(hi));
    asm volatile("lfence" ::: "memory");

    return ((uint64_t)hi << 32) | lo;
}

static void GetProcessorModelFamily(uint8_t &model, uint8_t &family)
{
    unsigned int eax, ebx, ecx, edx;
    unsigned int stepping, ecx_flags, edx_flags;

    eax = ebx = ecx = edx = 0;
    __get_cpuid(0x01, &eax, &ebx, &ecx, &edx);

    family = (eax >> 8) & 0x0f;
    model = (uint8_t)((eax >> 4) & 0x0f);

    if (family == 0x0f)
        family += (eax >> 20) & 0xff;

    if (family >= 6)
        model += ((eax >> 16) & 0xf) << 4;

    stepping = eax & 0x0f;
    ecx_flags = ecx;
    edx_flags = edx;

    fmt::print(
        stderr,
        "model: {0:x}, family: {0:x}, stepping: {0:x}, ecx_flags: {0:x}, edx_flags: {0:x}\n",
        model,
        family,
        stepping,
        ecx_flags,
        edx_flags);
}

static bool IsModelWestmere(uint8_t model)
{
    bool ret = false;
    switch (model)
    {
        /* Westmere */
        case INTEL_FAM6_WESTMERE:
        case INTEL_FAM6_WESTMERE_EP:
        case INTEL_FAM6_WESTMERE_EX:
            ret = true;
    }

    return ret;
}

static bool IsModelNehalem(uint8_t model)
{
    bool ret = false;

    switch (model)
    {
        /* Nehalem */
        case INTEL_FAM6_NEHALEM:
        case INTEL_FAM6_NEHALEM_G:
        case INTEL_FAM6_NEHALEM_EP:
        case INTEL_FAM6_NEHALEM_EX:
            ret = true;
    }

    return ret;
}

static bool ReadMsr(int msr, uint64_t *val)
{
    bool ret = false;

    const auto fd = open("/dev/cpu/0/msr", O_RDONLY);
    if (fd >= 0)
    {
        const auto result = pread(fd, val, sizeof(*val), msr);
        if (result == sizeof(*val))
        {
            ret = true;
        }

        close(fd);
    }

    return ret;
}

uint64_t GetTscHz(uint8_t model, unsigned int maxLevel)
{
    unsigned int eax, ebx, ecx, edx;
    uint64_t tsc_hz = 0;

    if (maxLevel >= 0x15)
    {
        eax = ebx = ecx = edx = 0;
        __get_cpuid(0x15, &eax, &ebx, &ecx, &edx);

        fmt::print(stderr, "CPUID(0x15): eax_crystal: {} ebx_tsc: {} ecx_crystal_hz: {}, edx {}\n", eax, ebx, ecx, edx);

        if (ebx != 0)
        {
            if (ecx == 0)
                switch (model)
                {
                    case INTEL_FAM6_SKYLAKE_L: /* SKL */
                    case INTEL_FAM6_SKYLAKE:   /* SKL */
                        ecx = 24000000U;       /* 24.0 MHz */
                        break;
                    case INTEL_FAM6_ATOM_GOLDMONT_D: /* DNV */
                        ecx = 25000000U;             /* 25.0 MHz */
                        break;
                    case INTEL_FAM6_ATOM_GOLDMONT: /* BXT */
                    case INTEL_FAM6_ATOM_GOLDMONT_PLUS:
                        ecx = 19200000U; /* 19.2 MHz */
                        break;
                    default:
                        ecx = 0U;
                }

            if (ecx)
            {
                tsc_hz = (uint64_t)ecx * ebx / eax;
                fmt::print(stderr, "TSC: {} MHz ({} Hz * {} / {} / 1000000)\n", tsc_hz / 1000000, ecx, ebx, eax);
            }
        }
    }

    if (tsc_hz == 0)
    {
        uint8_t mult = 0;
        if (IsModelWestmere(model) || IsModelNehalem(model))
            mult = 133;
        else
            mult = 100;

        uint64_t value = 0;
        const auto ret = ReadMsr(0xCE, &value);
        if (ret && value != 0)
        {
            const uint8_t reg = (value >> 8) & 0xff;
            tsc_hz = reg * mult * 1E6;
        }
    }

    return tsc_hz;
}

bool GetProcessorInfo(ProcessorInfo &info)
{
    unsigned int eax, ebx, ecx, edx;
    ProcessorVendor vendor = ProcessorVendor::UNKNOWN;

    eax = ebx = ecx = edx = 0;
    __get_cpuid(0, &eax, &ebx, &ecx, &edx);

    if (ebx == 0x756e6547 && ecx == 0x6c65746e && edx == 0x49656e69)
    {
        vendor = ProcessorVendor::INTEL;
    }
    else if (ebx == 0x68747541 && ecx == 0x444d4163 && edx == 0x69746e65)
    {
        vendor = ProcessorVendor::AMD;
    }
    else if (ebx == 0x6f677948 && ecx == 0x656e6975 && edx == 0x6e65476e)
    {
        vendor = ProcessorVendor::HYGON;
    }

    const unsigned int maxLevel = eax;
    char processorName[PROCESSOR_NAME_MAXLEN] = {0};

    memcpy(processorName, (char *)&ebx, sizeof(ebx));
    memcpy(processorName + sizeof(ebx), (char *)&edx, sizeof(edx));
    memcpy(processorName + sizeof(ebx) + sizeof(edx), (char *)&ecx, sizeof(ecx));
    processorName[sizeof(ebx) + sizeof(ecx) + sizeof(edx)] = '\0';

    fmt::print(stderr, "CPUID: {}, max level: {}\n", processorName, maxLevel);

    uint8_t model = 0;
    uint8_t family = 0;

    GetProcessorModelFamily(model, family);

    info.vendor = vendor;
    info.name = processorName;
    info.maxLevel = maxLevel;
    info.model = model;
    info.family = family;

    info.tscHz = GetTscHz(model, maxLevel);

    return true;
}

int main()
{
    using namespace std::chrono_literals;
    ProcessorInfo info = {};

    if (!GetProcessorInfo(info))
    {
        fmt::print(stderr, "Failed to get processor info");
        return -1;
    }

    //    const auto tsc_hz = get_tsc_hz();
    fmt::print(stderr, "TSC: {} \n", info.tscHz);

    while (true)
    {
        const auto t1 = ReadTsc();
        fmt::print(stderr, "ticks: {}\n", ReadTsc());
        std::this_thread::sleep_for(1000ms);
        const auto t2 = ReadTsc();
        fmt::print(stderr, "ticks for 1 second: {}\n", t2 - t1);

        const uint64_t diff = t2 - t1;
        const double duration = static_cast<double>(diff) / info.tscHz;
        fmt::print(stderr, "duration: {:f}\n", duration);
    }
}
