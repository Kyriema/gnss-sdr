// SPDX-FileCopyrightText: 2017 Google LLC
// SPDX-License-Identifier: Apache-2.0

#include "cpuinfo_aarch64.h"
#include "filesystem_for_testing.h"
#include "gtest/gtest.h"
#include "hwcaps_for_testing.h"
#include <set>
#if defined(CPU_FEATURES_OS_WINDOWS)
#include "internal/windows_utils.h"
#endif  // CPU_FEATURES_OS_WINDOWS
#if defined(CPU_FEATURES_OS_FREEBSD) || defined(CPU_FEATURES_OS_OPENBSD) || defined(CPU_FEATURES_OS_LINUX)
#include "internal/cpuid_aarch64.h"
#endif  // defined(CPU_FEATURES_OS_FREEBSD) || defined(CPU_FEATURES_OS_OPENBSD) || defined(CPU_FEATURES_OS_LINUX)

namespace cpu_features
{
class FakeCpuAarch64
{
#if defined(CPU_FEATURES_OS_FREEBSD) || defined(CPU_FEATURES_OS_OPENBSD) || defined(CPU_FEATURES_OS_LINUX)
public:
    uint64_t GetMidrEl1() const { return _midr_el1; }

    void SetMidrEl1(uint64_t midr_el1) { _midr_el1 = midr_el1; }

private:
    uint64_t _midr_el1;
#elif defined(CPU_FEATURES_OS_MACOS)
    std::set<std::string> darwin_sysctlbyname_;
    std::map<std::string, int> darwin_sysctlbynamevalue_;

public:
    bool GetDarwinSysCtlByName(std::string name) const
    {
        return darwin_sysctlbyname_.count(name);
    }

    int GetDarwinSysCtlByNameValue(std::string name) const
    {
        const auto iter = darwin_sysctlbynamevalue_.find(name);
        if (iter != darwin_sysctlbynamevalue_.end()) return iter->second;
        return 0;
    }

    void SetDarwinSysCtlByName(std::string name)
    {
        darwin_sysctlbyname_.insert(name);
    }

    void SetDarwinSysCtlByNameValue(std::string name, int value)
    {
        darwin_sysctlbynamevalue_[name] = value;
    }
#elif defined(CPU_FEATURES_OS_WINDOWS)
    std::set<DWORD> windows_isprocessorfeaturepresent_;
    WORD processor_revision_{};

public:
    bool GetWindowsIsProcessorFeaturePresent(DWORD dwProcessorFeature)
    {
        return windows_isprocessorfeaturepresent_.count(dwProcessorFeature);
    }

    void SetWindowsIsProcessorFeaturePresent(DWORD dwProcessorFeature)
    {
        windows_isprocessorfeaturepresent_.insert(dwProcessorFeature);
    }

    WORD GetWindowsNativeSystemInfoProcessorRevision() const
    {
        return processor_revision_;
    }

    void SetWindowsNativeSystemInfoProcessorRevision(WORD wProcessorRevision)
    {
        processor_revision_ = wProcessorRevision;
    }
#endif
};

static FakeCpuAarch64* g_fake_cpu_instance = nullptr;

static FakeCpuAarch64& cpu()
{
    assert(g_fake_cpu_instance != nullptr);
    return *g_fake_cpu_instance;
}

// Define OS dependent mock functions
#if defined(CPU_FEATURES_OS_FREEBSD) || defined(CPU_FEATURES_OS_OPENBSD) || defined(CPU_FEATURES_OS_LINUX)
extern "C" uint64_t GetMidrEl1(void) { return cpu().GetMidrEl1(); }
#elif defined(CPU_FEATURES_OS_MACOS)
extern "C" bool GetDarwinSysCtlByName(const char* name)
{
    return cpu().GetDarwinSysCtlByName(name);
}

extern "C" int GetDarwinSysCtlByNameValue(const char* name)
{
    return cpu().GetDarwinSysCtlByNameValue(name);
}
#elif defined(CPU_FEATURES_OS_WINDOWS)
extern "C" bool GetWindowsIsProcessorFeaturePresent(DWORD dwProcessorFeature)
{
    return cpu().GetWindowsIsProcessorFeaturePresent(dwProcessorFeature);
}

extern "C" WORD GetWindowsNativeSystemInfoProcessorRevision()
{
    return cpu().GetWindowsNativeSystemInfoProcessorRevision();
}
#endif

namespace
{

class CpuidAarch64Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        assert(g_fake_cpu_instance == nullptr);
        g_fake_cpu_instance = new FakeCpuAarch64();
    }
    void TearDown() override
    {
        delete g_fake_cpu_instance;
        g_fake_cpu_instance = nullptr;
    }
};

TEST_F(CpuidAarch64Test, Aarch64FeaturesEnum)
{
    const char* last_name = GetAarch64FeaturesEnumName(AARCH64_LAST_);
    EXPECT_STREQ(last_name, "unknown_feature");
    for (int i = static_cast<int>(AARCH64_FP);
        i != static_cast<int>(AARCH64_LAST_); ++i)
        {
            const auto feature = static_cast<Aarch64FeaturesEnum>(i);
            const char* name = GetAarch64FeaturesEnumName(feature);
            ASSERT_FALSE(name == nullptr);
            EXPECT_STRNE(name, "");
            EXPECT_STRNE(name, last_name);
        }
}

// AT_HWCAP tests
#if defined(CPU_FEATURES_OS_LINUX) || defined(CPU_FEATURES_OS_FREEBSD) || defined(CPU_FEATURES_OS_OPENBSD)
TEST_F(CpuidAarch64Test, FromHardwareCap)
{
    ResetHwcaps();
    SetHardwareCapabilities(AARCH64_HWCAP_FP | AARCH64_HWCAP_AES, 0);
    GetEmptyFilesystem();  // disabling /proc/cpuinfo
    const auto info = GetAarch64Info();
    EXPECT_TRUE(info.features.fp);
    EXPECT_FALSE(info.features.asimd);
    EXPECT_FALSE(info.features.evtstrm);
    EXPECT_TRUE(info.features.aes);
    EXPECT_FALSE(info.features.pmull);
    EXPECT_FALSE(info.features.sha1);
    EXPECT_FALSE(info.features.sha2);
    EXPECT_FALSE(info.features.crc32);
    EXPECT_FALSE(info.features.atomics);
    EXPECT_FALSE(info.features.fphp);
    EXPECT_FALSE(info.features.asimdhp);
    EXPECT_FALSE(info.features.cpuid);
    EXPECT_FALSE(info.features.asimdrdm);
    EXPECT_FALSE(info.features.jscvt);
    EXPECT_FALSE(info.features.fcma);
    EXPECT_FALSE(info.features.lrcpc);
    EXPECT_FALSE(info.features.dcpop);
    EXPECT_FALSE(info.features.sha3);
    EXPECT_FALSE(info.features.sm3);
    EXPECT_FALSE(info.features.sm4);
    EXPECT_FALSE(info.features.asimddp);
    EXPECT_FALSE(info.features.sha512);
    EXPECT_FALSE(info.features.sve);
    EXPECT_FALSE(info.features.asimdfhm);
    EXPECT_FALSE(info.features.dit);
    EXPECT_FALSE(info.features.uscat);
    EXPECT_FALSE(info.features.ilrcpc);
    EXPECT_FALSE(info.features.flagm);
    EXPECT_FALSE(info.features.ssbs);
    EXPECT_FALSE(info.features.sb);
    EXPECT_FALSE(info.features.paca);
    EXPECT_FALSE(info.features.pacg);
}

TEST_F(CpuidAarch64Test, FromHardwareCap2)
{
    ResetHwcaps();
    SetHardwareCapabilities(AARCH64_HWCAP_FP,
        AARCH64_HWCAP2_SVE2 | AARCH64_HWCAP2_BTI);
    GetEmptyFilesystem();  // disabling /proc/cpuinfo
    const auto info = GetAarch64Info();
    EXPECT_TRUE(info.features.fp);

    EXPECT_TRUE(info.features.sve2);
    EXPECT_TRUE(info.features.bti);

    EXPECT_FALSE(info.features.dcpodp);
    EXPECT_FALSE(info.features.sveaes);
    EXPECT_FALSE(info.features.svepmull);
    EXPECT_FALSE(info.features.svebitperm);
    EXPECT_FALSE(info.features.svesha3);
    EXPECT_FALSE(info.features.svesm4);
    EXPECT_FALSE(info.features.flagm2);
    EXPECT_FALSE(info.features.frint);
    EXPECT_FALSE(info.features.svei8mm);
    EXPECT_FALSE(info.features.svef32mm);
    EXPECT_FALSE(info.features.svef64mm);
    EXPECT_FALSE(info.features.svebf16);
    EXPECT_FALSE(info.features.i8mm);
    EXPECT_FALSE(info.features.bf16);
    EXPECT_FALSE(info.features.dgh);
    EXPECT_FALSE(info.features.rng);
}
#endif  // defined(CPU_FEATURES_OS_LINUX) || defined(CPU_FEATURES_OS_FREEBSD) || defined(CPU_FEATURES_OS_OPENBSD)

// OS dependent tests
#if defined(CPU_FEATURES_OS_LINUX)
TEST_F(CpuidAarch64Test, ARMCortexA53)
{
    ResetHwcaps();
    auto& fs = GetEmptyFilesystem();
    fs.CreateFile("/proc/cpuinfo",
        R"(Processor   : AArch64 Processor rev 3 (aarch64)
processor   : 0
processor   : 1
processor   : 2
processor   : 3
processor   : 4
processor   : 5
processor   : 6
processor   : 7
Features    : fp asimd evtstrm aes pmull sha1 sha2 crc32
CPU implementer : 0x41
CPU architecture: AArch64
CPU variant : 0x0
CPU part    : 0xd03
CPU revision    : 3)");
    const auto info = GetAarch64Info();
    EXPECT_EQ(info.implementer, 0x41);
    EXPECT_EQ(info.variant, 0x0);
    EXPECT_EQ(info.part, 0xd03);
    EXPECT_EQ(info.revision, 3);

    EXPECT_TRUE(info.features.fp);
    EXPECT_TRUE(info.features.asimd);
    EXPECT_TRUE(info.features.evtstrm);
    EXPECT_TRUE(info.features.aes);
    EXPECT_TRUE(info.features.pmull);
    EXPECT_TRUE(info.features.sha1);
    EXPECT_TRUE(info.features.sha2);
    EXPECT_TRUE(info.features.crc32);

    EXPECT_FALSE(info.features.atomics);
    EXPECT_FALSE(info.features.fphp);
    EXPECT_FALSE(info.features.asimdhp);
    EXPECT_FALSE(info.features.cpuid);
    EXPECT_FALSE(info.features.asimdrdm);
    EXPECT_FALSE(info.features.jscvt);
    EXPECT_FALSE(info.features.fcma);
    EXPECT_FALSE(info.features.lrcpc);
    EXPECT_FALSE(info.features.dcpop);
    EXPECT_FALSE(info.features.sha3);
    EXPECT_FALSE(info.features.sm3);
    EXPECT_FALSE(info.features.sm4);
    EXPECT_FALSE(info.features.asimddp);
    EXPECT_FALSE(info.features.sha512);
    EXPECT_FALSE(info.features.sve);
    EXPECT_FALSE(info.features.asimdfhm);
    EXPECT_FALSE(info.features.dit);
    EXPECT_FALSE(info.features.uscat);
    EXPECT_FALSE(info.features.ilrcpc);
    EXPECT_FALSE(info.features.flagm);
    EXPECT_FALSE(info.features.ssbs);
    EXPECT_FALSE(info.features.sb);
    EXPECT_FALSE(info.features.paca);
    EXPECT_FALSE(info.features.pacg);
    EXPECT_FALSE(info.features.dcpodp);
    EXPECT_FALSE(info.features.sve2);
    EXPECT_FALSE(info.features.sveaes);
    EXPECT_FALSE(info.features.svepmull);
    EXPECT_FALSE(info.features.svebitperm);
    EXPECT_FALSE(info.features.svesha3);
    EXPECT_FALSE(info.features.svesm4);
    EXPECT_FALSE(info.features.flagm2);
    EXPECT_FALSE(info.features.frint);
    EXPECT_FALSE(info.features.svei8mm);
    EXPECT_FALSE(info.features.svef32mm);
    EXPECT_FALSE(info.features.svef64mm);
    EXPECT_FALSE(info.features.svebf16);
    EXPECT_FALSE(info.features.i8mm);
    EXPECT_FALSE(info.features.bf16);
    EXPECT_FALSE(info.features.dgh);
    EXPECT_FALSE(info.features.rng);
    EXPECT_FALSE(info.features.bti);
    EXPECT_FALSE(info.features.mte);
    EXPECT_FALSE(info.features.ecv);
    EXPECT_FALSE(info.features.afp);
    EXPECT_FALSE(info.features.rpres);
    EXPECT_FALSE(info.features.mte3);
    EXPECT_FALSE(info.features.sme);
    EXPECT_FALSE(info.features.smei16i64);
    EXPECT_FALSE(info.features.smef64f64);
    EXPECT_FALSE(info.features.smei8i32);
    EXPECT_FALSE(info.features.smef16f32);
    EXPECT_FALSE(info.features.smeb16f32);
    EXPECT_FALSE(info.features.smef32f32);
    EXPECT_FALSE(info.features.smefa64);
    EXPECT_FALSE(info.features.wfxt);
    EXPECT_FALSE(info.features.ebf16);
    EXPECT_FALSE(info.features.sveebf16);
    EXPECT_FALSE(info.features.cssc);
    EXPECT_FALSE(info.features.rprfm);
    EXPECT_FALSE(info.features.sve2p1);
    EXPECT_FALSE(info.features.sme2);
    EXPECT_FALSE(info.features.sme2p1);
    EXPECT_FALSE(info.features.smei16i32);
    EXPECT_FALSE(info.features.smebi32i32);
    EXPECT_FALSE(info.features.smeb16b16);
    EXPECT_FALSE(info.features.smef16f16);
    EXPECT_FALSE(info.features.mops);
    EXPECT_FALSE(info.features.hbc);
    EXPECT_FALSE(info.features.sveb16b16);
    EXPECT_FALSE(info.features.lrcpc3);
    EXPECT_FALSE(info.features.lse128);
    EXPECT_FALSE(info.features.fpmr);
    EXPECT_FALSE(info.features.lut);
    EXPECT_FALSE(info.features.faminmax);
    EXPECT_FALSE(info.features.f8cvt);
    EXPECT_FALSE(info.features.f8fma);
    EXPECT_FALSE(info.features.f8dp4);
    EXPECT_FALSE(info.features.f8dp2);
    EXPECT_FALSE(info.features.f8e4m3);
    EXPECT_FALSE(info.features.f8e5m2);
    EXPECT_FALSE(info.features.smelutv2);
    EXPECT_FALSE(info.features.smef8f16);
    EXPECT_FALSE(info.features.smef8f32);
    EXPECT_FALSE(info.features.smesf8fma);
    EXPECT_FALSE(info.features.smesf8dp4);
    EXPECT_FALSE(info.features.smesf8dp2);
}
#elif defined(CPU_FEATURES_OS_MACOS)
TEST_F(CpuidAarch64Test, FromDarwinSysctlFromName)
{
    cpu().SetDarwinSysCtlByName("hw.optional.floatingpoint");
    cpu().SetDarwinSysCtlByName("hw.optional.neon");
    cpu().SetDarwinSysCtlByName("hw.optional.AdvSIMD_HPFPCvt");
    cpu().SetDarwinSysCtlByName("hw.optional.arm.FEAT_FP16");
    cpu().SetDarwinSysCtlByName("hw.optional.arm.FEAT_LSE");
    cpu().SetDarwinSysCtlByName("hw.optional.armv8_crc32");
    cpu().SetDarwinSysCtlByName("hw.optional.arm.FEAT_FHM");
    cpu().SetDarwinSysCtlByName("hw.optional.arm.FEAT_SHA512");
    cpu().SetDarwinSysCtlByName("hw.optional.arm.FEAT_SHA3");
    cpu().SetDarwinSysCtlByName("hw.optional.amx_version");
    cpu().SetDarwinSysCtlByName("hw.optional.ucnormal_mem");
    cpu().SetDarwinSysCtlByName("hw.optional.arm64");

    cpu().SetDarwinSysCtlByNameValue("hw.cputype", 16777228);
    cpu().SetDarwinSysCtlByNameValue("hw.cpusubtype", 2);
    cpu().SetDarwinSysCtlByNameValue("hw.cpu64bit", 1);
    cpu().SetDarwinSysCtlByNameValue("hw.cpufamily", 458787763);
    cpu().SetDarwinSysCtlByNameValue("hw.cpusubfamily", 2);

    const auto info = GetAarch64Info();

    EXPECT_EQ(info.implementer, 0x100000C);
    EXPECT_EQ(info.variant, 2);
    EXPECT_EQ(info.part, 0x1B588BB3);
    EXPECT_EQ(info.revision, 2);

    EXPECT_TRUE(info.features.fp);
    EXPECT_FALSE(info.features.asimd);
    EXPECT_FALSE(info.features.evtstrm);
    EXPECT_FALSE(info.features.aes);
    EXPECT_FALSE(info.features.pmull);
    EXPECT_FALSE(info.features.sha1);
    EXPECT_FALSE(info.features.sha2);
    EXPECT_TRUE(info.features.crc32);
    EXPECT_TRUE(info.features.atomics);
    EXPECT_TRUE(info.features.fphp);
    EXPECT_FALSE(info.features.asimdhp);
    EXPECT_FALSE(info.features.cpuid);
    EXPECT_FALSE(info.features.asimdrdm);
    EXPECT_FALSE(info.features.jscvt);
    EXPECT_FALSE(info.features.fcma);
    EXPECT_FALSE(info.features.lrcpc);
    EXPECT_FALSE(info.features.dcpop);
    EXPECT_TRUE(info.features.sha3);
    EXPECT_FALSE(info.features.sm3);
    EXPECT_FALSE(info.features.sm4);
    EXPECT_FALSE(info.features.asimddp);
    EXPECT_TRUE(info.features.sha512);
    EXPECT_FALSE(info.features.sve);
    EXPECT_TRUE(info.features.asimdfhm);
    EXPECT_FALSE(info.features.dit);
    EXPECT_FALSE(info.features.uscat);
    EXPECT_FALSE(info.features.ilrcpc);
    EXPECT_FALSE(info.features.flagm);
    EXPECT_FALSE(info.features.ssbs);
    EXPECT_FALSE(info.features.sb);
    EXPECT_FALSE(info.features.paca);
    EXPECT_FALSE(info.features.pacg);
}
#elif defined(CPU_FEATURES_OS_WINDOWS)
TEST_F(CpuidAarch64Test, WINDOWS_AARCH64_RPI4)
{
    cpu().SetWindowsNativeSystemInfoProcessorRevision(0x03);
    cpu().SetWindowsIsProcessorFeaturePresent(PF_ARM_VFP_32_REGISTERS_AVAILABLE);
    cpu().SetWindowsIsProcessorFeaturePresent(PF_ARM_NEON_INSTRUCTIONS_AVAILABLE);
    cpu().SetWindowsIsProcessorFeaturePresent(
        PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE);

    const auto info = GetAarch64Info();

    EXPECT_EQ(info.revision, 0x03);
    EXPECT_TRUE(info.features.fp);
    EXPECT_TRUE(info.features.asimd);
    EXPECT_TRUE(info.features.crc32);
    EXPECT_FALSE(info.features.aes);
    EXPECT_FALSE(info.features.sha1);
    EXPECT_FALSE(info.features.sha2);
    EXPECT_FALSE(info.features.pmull);
    EXPECT_FALSE(info.features.atomics);
    EXPECT_FALSE(info.features.asimddp);
    EXPECT_FALSE(info.features.jscvt);
    EXPECT_FALSE(info.features.lrcpc);
}
#elif defined(CPU_FEATURES_OS_FREEBSD) || defined(CPU_FEATURES_OS_OPENBSD)
TEST_F(CpuidAarch64Test, MrsMidrEl1_RPI4)
{
    ResetHwcaps();
    SetHardwareCapabilities(AARCH64_HWCAP_FP | AARCH64_HWCAP_CPUID, 0);
    cpu().SetMidrEl1(0x410FD083);
    const auto info = GetAarch64Info();

    EXPECT_EQ(info.implementer, 0x41);
    EXPECT_EQ(info.variant, 0);
    EXPECT_EQ(info.part, 0xD08);
    EXPECT_EQ(info.revision, 0x3);

    EXPECT_TRUE(info.features.fp);

    EXPECT_FALSE(info.features.dcpodp);
    EXPECT_FALSE(info.features.sveaes);
    EXPECT_FALSE(info.features.svepmull);
    EXPECT_FALSE(info.features.svebitperm);
    EXPECT_FALSE(info.features.svesha3);
    EXPECT_FALSE(info.features.svesm4);
    EXPECT_FALSE(info.features.flagm2);
    EXPECT_FALSE(info.features.frint);
    EXPECT_FALSE(info.features.svei8mm);
    EXPECT_FALSE(info.features.svef32mm);
    EXPECT_FALSE(info.features.svef64mm);
    EXPECT_FALSE(info.features.svebf16);
    EXPECT_FALSE(info.features.i8mm);
    EXPECT_FALSE(info.features.bf16);
    EXPECT_FALSE(info.features.dgh);
    EXPECT_FALSE(info.features.rng);
}
#endif  // CPU_FEATURES_OS_FREEBSD
}  // namespace
}  // namespace cpu_features