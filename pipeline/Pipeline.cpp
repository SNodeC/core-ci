#include "Pipeline.h"

namespace coreci {
    Pipeline productionPipeline() {
        // Matrix sources: snode.c 76ca64775513a0c1bd94d9a73e9ea64a495c4305;
        // mqttsuite 35a4f78c145d99e2aa12b673a1617b0c59fdbc2b. Only matrix ids and family mapping were used.
        Pipeline::Builder builder;
        builder.feed({"packages", {"SNodeC", "DistributionPackages", "coreci-publish.yml", "main"}});
        const std::vector<Target> matrix{
            {"debian-trixie-amd64", Family::Debian, "packages"},
            {"debian-trixie-arm64", Family::Debian, "packages"},
            {"debian-trixie-armhf", Family::Debian, "packages"},
            {"debian-trixie-riscv64", Family::Debian, "packages"},
            {"debian-forky-amd64", Family::Debian, "packages"},
            {"debian-forky-arm64", Family::Debian, "packages"},
            {"debian-forky-armhf", Family::Debian, "packages"},
            {"debian-forky-riscv64", Family::Debian, "packages"},
            {"debian-sid-amd64", Family::Debian, "packages"},
            {"debian-sid-arm64", Family::Debian, "packages"},
            {"debian-sid-armhf", Family::Debian, "packages"},
            {"debian-sid-riscv64", Family::Debian, "packages"},
            {"ubuntu-noble-amd64", Family::Ubuntu, "packages"},
            {"ubuntu-noble-arm64", Family::Ubuntu, "packages"},
            {"ubuntu-resolute-amd64", Family::Ubuntu, "packages"},
            {"ubuntu-resolute-arm64", Family::Ubuntu, "packages"},
            {"rocky-9-x86_64", Family::RockyLinux, "packages"},
            {"rocky-9-aarch64", Family::RockyLinux, "packages"},
            {"rocky-10-x86_64", Family::RockyLinux, "packages"},
            {"rocky-10-aarch64", Family::RockyLinux, "packages"},
            {"fedora-43-x86_64", Family::Fedora, "packages"},
            {"fedora-43-aarch64", Family::Fedora, "packages"},
            {"fedora-44-x86_64", Family::Fedora, "packages"},
            {"fedora-44-aarch64", Family::Fedora, "packages"},
            {"openwrt-24.10-x86_64", Family::OpenWrtOpkg, "packages"},
            {"openwrt-24.10-i386_pentium-mmx", Family::OpenWrtOpkg, "packages"},
            {"openwrt-24.10-i386_pentium4", Family::OpenWrtOpkg, "packages"},
            {"openwrt-24.10-aarch64_cortex-a53", Family::OpenWrtOpkg, "packages"},
            {"openwrt-24.10-aarch64_cortex-a72", Family::OpenWrtOpkg, "packages"},
            {"openwrt-24.10-aarch64_cortex-a76", Family::OpenWrtOpkg, "packages"},
            {"openwrt-24.10-aarch64_generic", Family::OpenWrtOpkg, "packages"},
            {"openwrt-24.10-arm_cortex-a5_vfpv4", Family::OpenWrtOpkg, "packages"},
            {"openwrt-24.10-arm_cortex-a7", Family::OpenWrtOpkg, "packages"},
            {"openwrt-24.10-arm_cortex-a7_neon-vfpv4", Family::OpenWrtOpkg, "packages"},
            {"openwrt-24.10-arm_cortex-a7_vfpv4", Family::OpenWrtOpkg, "packages"},
            {"openwrt-24.10-arm_cortex-a8_vfpv3", Family::OpenWrtOpkg, "packages"},
            {"openwrt-24.10-arm_cortex-a9", Family::OpenWrtOpkg, "packages"},
            {"openwrt-24.10-arm_cortex-a9_neon", Family::OpenWrtOpkg, "packages"},
            {"openwrt-24.10-arm_cortex-a9_vfpv3-d16", Family::OpenWrtOpkg, "packages"},
            {"openwrt-24.10-arm_cortex-a15_neon-vfpv4", Family::OpenWrtOpkg, "packages"},
            {"openwrt-24.10-loongarch64_generic", Family::OpenWrtOpkg, "packages"},
            {"openwrt-24.10-mips64_octeonplus", Family::OpenWrtOpkg, "packages"},
            {"openwrt-24.10-mips_24kc", Family::OpenWrtOpkg, "packages"},
            {"openwrt-24.10-mipsel_24kc", Family::OpenWrtOpkg, "packages"},
            {"openwrt-24.10-mipsel_24kc_24kf", Family::OpenWrtOpkg, "packages"},
            {"openwrt-24.10-mipsel_74kc", Family::OpenWrtOpkg, "packages"},
            {"openwrt-24.10-powerpc_464fp", Family::OpenWrtOpkg, "packages"},
            {"openwrt-24.10-powerpc_8548", Family::OpenWrtOpkg, "packages"},
            {"openwrt-24.10-riscv64_generic", Family::OpenWrtOpkg, "packages"},
            {"openwrt-25.12-x86_64", Family::OpenWrtApk, "packages"},
            {"openwrt-25.12-i386_pentium-mmx", Family::OpenWrtApk, "packages"},
            {"openwrt-25.12-i386_pentium4", Family::OpenWrtApk, "packages"},
            {"openwrt-25.12-aarch64_cortex-a53", Family::OpenWrtApk, "packages"},
            {"openwrt-25.12-aarch64_cortex-a72", Family::OpenWrtApk, "packages"},
            {"openwrt-25.12-aarch64_cortex-a76", Family::OpenWrtApk, "packages"},
            {"openwrt-25.12-aarch64_generic", Family::OpenWrtApk, "packages"},
            {"openwrt-25.12-arm_cortex-a5_vfpv4", Family::OpenWrtApk, "packages"},
            {"openwrt-25.12-arm_cortex-a7", Family::OpenWrtApk, "packages"},
            {"openwrt-25.12-arm_cortex-a7_neon-vfpv4", Family::OpenWrtApk, "packages"},
            {"openwrt-25.12-arm_cortex-a7_vfpv4", Family::OpenWrtApk, "packages"},
            {"openwrt-25.12-arm_cortex-a8_vfpv3", Family::OpenWrtApk, "packages"},
            {"openwrt-25.12-arm_cortex-a9", Family::OpenWrtApk, "packages"},
            {"openwrt-25.12-arm_cortex-a9_neon", Family::OpenWrtApk, "packages"},
            {"openwrt-25.12-arm_cortex-a9_vfpv3-d16", Family::OpenWrtApk, "packages"},
            {"openwrt-25.12-arm_cortex-a15_neon-vfpv4", Family::OpenWrtApk, "packages"},
            {"openwrt-25.12-loongarch64_generic", Family::OpenWrtApk, "packages"},
            {"openwrt-25.12-mips64_octeonplus", Family::OpenWrtApk, "packages"},
            {"openwrt-25.12-mips_24kc", Family::OpenWrtApk, "packages"},
            {"openwrt-25.12-mipsel_24kc", Family::OpenWrtApk, "packages"},
            {"openwrt-25.12-mipsel_24kc_24kf", Family::OpenWrtApk, "packages"},
            {"openwrt-25.12-mipsel_74kc", Family::OpenWrtApk, "packages"},
            {"openwrt-25.12-powerpc_464fp", Family::OpenWrtApk, "packages"},
            {"openwrt-25.12-powerpc_8548", Family::OpenWrtApk, "packages"},
            {"openwrt-25.12-riscv64_generic", Family::OpenWrtApk, "packages"},
            {"raspberrypios-bookworm-arm64", Family::RaspberryPiOS, "packages"},
            {"raspberrypios-trixie-arm64", Family::RaspberryPiOS, "packages"},
        };
        std::set<std::string> targets;
        for (const auto& target : matrix) {
            builder.target(target);
            targets.insert(target.id);
        }
        builder.package({"snode.c", {"SNodeC", "snode.c"}, {"SNodeC", "DistributionPackages", "coreci-build.yml", "main"}, targets, {}});
        builder.package(
            {"mqttsuite", {"SNodeC", "mqttsuite"}, {"SNodeC", "DistributionPackages", "coreci-build.yml", "main"}, targets, {"snode.c"}});
        return builder.build();
    }
} // namespace coreci
