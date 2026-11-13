#include <jni.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <android/log.h>

#define LOG_TAG "AmongUsVIPMod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace RVA {
    constexpr uintptr_t LogicOptions_GetKillCooldown     = 0x20D3BBC;
    constexpr uintptr_t PlayerControl_IsKillTimerEnabled = 0x21D1AA8;
    constexpr uintptr_t ShipStatus_CalculateLight        = 0x2253108;
    constexpr uintptr_t AirshipStatus_CalculateLight     = 0x20D218C;
    constexpr uintptr_t RoleBehaviour_IsAffectedByComms  = 0x221A004;
    constexpr uintptr_t PlayerControl_get_CanMove        = 0x21CA164;
    constexpr uintptr_t LogicOptions_GetKillDistance     = 0x20D3C90;
    constexpr uintptr_t RoleBehaviour_GetAbilityDistance = 0x221A4E4;
    constexpr uintptr_t RoleBehaviour_CanUse             = 0x221A050;
    constexpr uintptr_t PlayerControl_MustCleanVent      = 0x21D5D7C;
    constexpr uintptr_t LogicOptions_GetEngineerCooldown = 0x20D43B8;
    constexpr uintptr_t LogicOptions_GetEngineerInVent   = 0x20D44A0;
    constexpr uintptr_t LogicOptions_GetEmergencyCD      = 0x20D3FA0;
    constexpr uintptr_t LogicOptions_GetNumEmergencies   = 0x20D4070;
    constexpr uintptr_t LogicOptions_GetShowCrewNames    = 0x20D466C;
    constexpr uintptr_t PlayerControl_IsMushroomMixup    = 0x21D8B20;
    constexpr uintptr_t PlayerControl_AreCommsAffected   = 0x21D8D00;
    constexpr uintptr_t RoleBehaviour_get_IsImpostor     = 0x2213D08;
    constexpr uintptr_t RoleManager_IsImpostorRole       = 0x221C444;
    constexpr uintptr_t Ladder_IsCoolingDown             = 0x21ACBE0;
    constexpr uintptr_t Ladder_get_CoolDown              = 0x21ACBD0;
    constexpr uintptr_t LogicOptions_GetGhostsDoTasks    = 0x20D3AE8;
    constexpr uintptr_t PlayerControl_AllTasksCompleted  = 0x21D5C9C;
    constexpr uintptr_t LogicOptions_GetConfirmImpostor  = 0x20D3ECC;
    constexpr uintptr_t LogicOptions_GetVisualTasks      = 0x20D4140;
    constexpr uintptr_t InnerNetClient_CanBan            = 0x231A5A8;
    constexpr uintptr_t InnerNetClient_CanKick           = 0x231A5CC;
    constexpr uintptr_t EOSManager_IsFreechatAllowed     = 0x20A78F4;
    constexpr uintptr_t EOSManager_IsFriendsListAllowed  = 0x20A9728;
    constexpr uintptr_t AccountMgr_CanMinorSetCustomName = 0x20A81F4;
    constexpr uintptr_t PlayerPhysics_get_TrueSpeed      = 0x21E4990;
    constexpr uintptr_t PlayerPhysics_get_SpeedMod       = 0x21E49B0;
    constexpr uintptr_t LogicOptions_GetPlayerSpeedMod   = 0x20D3DF8;
    constexpr uintptr_t Constants_IsVersionModded        = 0x22B7804;
}

static int g_targetPid = -1;
static uintptr_t g_il2cppBase = 0;
static uintptr_t g_il2cppExecBase = 0;
static int g_memFd = -1;
static int g_patchCount = 0;
static int g_failCount = 0;

static std::unordered_map<uintptr_t, std::vector<uint8_t>> g_originalBytes;

int FindGamePid(const char* packageName) {
    DIR* dir = opendir("/proc");
    if (!dir) return -1;
    struct dirent* entry;
    char path[256], cmdline[256];

    while ((entry = readdir(dir)) != nullptr) {
        int pid = atoi(entry->d_name);
        if (pid <= 0) continue;
        snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
        FILE* fp = fopen(path, "r");
        if (fp) {
            if (fgets(cmdline, sizeof(cmdline), fp) != nullptr) {
                if (strstr(cmdline, packageName) != nullptr) {
                    fclose(fp);
                    closedir(dir);
                    return pid;
                }
            }
            fclose(fp);
        }
    }
    closedir(dir);
    return -1;
}

void FindIl2CppBases(int pid, uintptr_t& outBase, uintptr_t& outExecBase) {
    outBase = 0;
    outExecBase = 0;
    char path[256], line[512];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    FILE* fp = fopen(path, "r");
    if (!fp) return;

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "libil2cpp.so")) {
            uintptr_t startAddr = strtoull(line, nullptr, 16);
            if (outBase == 0) outBase = startAddr;
            if (strstr(line, "r-xp") && outExecBase == 0) {
                outExecBase = startAddr;
            }
        }
    }
    fclose(fp);
}

bool OpenMemFd(int pid) {
    if (g_memFd >= 0) { close(g_memFd); g_memFd = -1; }
    char memPath[64];
    snprintf(memPath, sizeof(memPath), "/proc/%d/mem", pid);
    g_memFd = open(memPath, O_RDWR);
    if (g_memFd < 0) {
        char cmd[160];
        snprintf(cmd, sizeof(cmd), "su -c 'chmod 666 /proc/%d/mem' 2>/dev/null", pid);
        system(cmd);
        g_memFd = open(memPath, O_RDWR);
    }
    return g_memFd >= 0;
}

// Android 15 & SELinux Bypass: ptrace ile bellek yazma
bool PtraceWrite(int pid, uintptr_t addr, const void* data, size_t size) {
    if (ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) == -1) return false;
    waitpid(pid, nullptr, 0);

    bool success = true;
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    size_t written = 0;

    while (written < size) {
        uintptr_t currentAddr = addr + written;
        long word = 0;
        
        if (size - written >= sizeof(long)) {
            memcpy(&word, bytes + written, sizeof(long));
            if (ptrace(PTRACE_POKEDATA, pid, reinterpret_cast<void*>(currentAddr), reinterpret_cast<void*>(word)) == -1) {
                success = false; break;
            }
            written += sizeof(long);
        } else {
            long original = ptrace(PTRACE_PEEKDATA, pid, reinterpret_cast<void*>(currentAddr), nullptr);
            if (original == -1) { success = false; break; }
            memcpy(&original, bytes + written, size - written);
            if (ptrace(PTRACE_POKEDATA, pid, reinterpret_cast<void*>(currentAddr), reinterpret_cast<void*>(original)) == -1) {
                success = false; break;
            }
            written += (size - written);
        }
    }

    ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
    return success;
}

bool RawWrite(uintptr_t address, const void* buffer, size_t size) {
    if (g_targetPid <= 0) return false;

    // ptrace ile yazmayı zorla (SELinux'u atlar)
    if (PtraceWrite(g_targetPid, address, buffer, size)) {
        g_patchCount++;
        return true;
    }

    // Yedek: /proc/pid/mem
    if (g_memFd >= 0) {
        ssize_t written = pwrite64(g_memFd, buffer, size, static_cast<off64_t>(address));
        if (written == static_cast<ssize_t>(size)) {
            g_patchCount++;
            return true;
        }
    }

    g_failCount++;
    return false;
}

bool RawRead(uintptr_t address, void* buffer, size_t size) {
    if (g_targetPid <= 0) return false;
    
    if (ptrace(PTRACE_ATTACH, g_targetPid, nullptr, nullptr) == -1) return false;
    waitpid(g_targetPid, nullptr, 0);
    
    bool success = true;
    size_t read_bytes = 0;
    uint8_t* bytes = static_cast<uint8_t*>(buffer);
    
    while (read_bytes < size) {
        long word = ptrace(PTRACE_PEEKDATA, g_targetPid, reinterpret_cast<void*>(address + read_bytes), nullptr);
        if (word == -1) { success = false; break; }
        size_t to_copy = (size - read_bytes >= sizeof(long)) ? sizeof(long) : (size - read_bytes);
        memcpy(bytes + read_bytes, &word, to_copy);
        read_bytes += to_copy;
    }
    
    ptrace(PTRACE_DETACH, g_targetPid, nullptr, nullptr);
    return success;
}

void PatchRVA(uintptr_t rva, const void* patchData, size_t size, bool enable) {
    if (g_il2cppBase == 0) return;

    std::vector<uintptr_t> targets;
    targets.push_back(g_il2cppBase + rva);
    if (rva > 0x4000) {
        targets.push_back(g_il2cppBase + (rva - 0x4000));
    }
    if (g_il2cppExecBase != 0 && g_il2cppExecBase != g_il2cppBase) {
        uintptr_t execDiff = g_il2cppExecBase - g_il2cppBase;
        if (rva > execDiff) {
            targets.push_back(g_il2cppExecBase + (rva - execDiff));
        }
        targets.push_back(g_il2cppExecBase + rva);
    }

    for (uintptr_t addr : targets) {
        if (enable) {
            if (g_originalBytes.find(addr) == g_originalBytes.end()) {
                std::vector<uint8_t> orig(size);
                if (RawRead(addr, orig.data(), size)) {
                    g_originalBytes[addr] = orig;
                }
            }
            RawWrite(addr, patchData, size);
        } else {
            auto it = g_originalBytes.find(addr);
            if (it != g_originalBytes.end()) {
                RawWrite(addr, it->second.data(), it->second.size());
            }
        }
    }
}

void PatchFloatReturn(uintptr_t rva, float val, bool enable) {
    uint32_t floatBits = 0;
    memcpy(&floatBits, &val, sizeof(float));
    uint32_t code[3] = { 0x1C000040, 0xD65F03C0, floatBits };
    PatchRVA(rva, code, sizeof(code), enable);
}

void PatchIntReturn(uintptr_t rva, uint16_t val, bool enable) {
    uint32_t movInstr = 0x52800000 | ((val & 0xFFFF) << 5);
    uint32_t code[2] = { movInstr, 0xD65F03C0 };
    PatchRVA(rva, code, sizeof(code), enable);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_external_amongusmod_FloatingModService_attachToGame(JNIEnv* env, jobject thiz) {
    int pid = FindGamePid("com.innersloth.spacemafia");
    if (pid <= 0) {
        g_targetPid = -1;
        g_il2cppBase = 0;
        g_il2cppExecBase = 0;
        g_originalBytes.clear();
        if (g_memFd >= 0) { close(g_memFd); g_memFd = -1; }
        return JNI_FALSE;
    }

    uintptr_t base = 0, execBase = 0;
    FindIl2CppBases(pid, base, execBase);
    if (base == 0) return JNI_FALSE;

    if (g_targetPid != pid || g_memFd < 0) {
        OpenMemFd(pid);
    }

    g_targetPid = pid;
    g_il2cppBase = base;
    g_il2cppExecBase = execBase;

    return JNI_TRUE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_external_amongusmod_FloatingModService_getConnectionInfo(JNIEnv* env, jobject thiz) {
    char info[256];
    if (g_targetPid > 0 && g_il2cppBase > 0) {
        snprintf(info, sizeof(info),
                 "AKTİF | PID:%d | Base:0x%lx\nSELinux Bypass | OK:%d FAIL:%d",
                 g_targetPid,
                 (unsigned long)g_il2cppBase,
                 g_patchCount,
                 g_failCount);
    } else {
        snprintf(info, sizeof(info), "OYUN BEKLENİYOR...");
    }
    return env->NewStringUTF(info);
}

extern "C" JNIEXPORT void JNICALL
Java_com_external_amongusmod_FloatingModService_applyFeaturePatch(
        JNIEnv* env, jobject thiz, jint featureId, jboolean enabled, jfloat value) {
    if (g_il2cppBase == 0) return;

    PatchIntReturn(RVA::Constants_IsVersionModded, 0, true);

    switch (featureId) {
        case 1:
            PatchFloatReturn(RVA::LogicOptions_GetKillCooldown, 0.01f, enabled);
            PatchIntReturn(RVA::PlayerControl_IsKillTimerEnabled, 1, enabled);
            break;
        case 2:
            PatchFloatReturn(RVA::ShipStatus_CalculateLight, 50.0f, enabled);
            PatchFloatReturn(RVA::AirshipStatus_CalculateLight, 50.0f, enabled);
            PatchIntReturn(RVA::RoleBehaviour_IsAffectedByComms, 0, enabled);
            break;
        case 3:
            PatchIntReturn(RVA::PlayerControl_get_CanMove, 1, enabled);
            break;
        case 4:
            PatchFloatReturn(RVA::LogicOptions_GetKillDistance, 99.0f, enabled);
            PatchFloatReturn(RVA::RoleBehaviour_GetAbilityDistance, 99.0f, enabled);
            break;
        case 5:
            PatchIntReturn(RVA::RoleBehaviour_CanUse, 1, enabled);
            PatchIntReturn(RVA::PlayerControl_MustCleanVent, 0, enabled);
            break;
        case 6:
            PatchFloatReturn(RVA::LogicOptions_GetEngineerCooldown, 0.0f, enabled);
            PatchFloatReturn(RVA::LogicOptions_GetEngineerInVent, 9999.0f, enabled);
            break;
        case 7:
            PatchIntReturn(RVA::LogicOptions_GetEmergencyCD, 0, enabled);
            PatchIntReturn(RVA::LogicOptions_GetNumEmergencies, 99, enabled);
            break;
        case 8:
            PatchIntReturn(RVA::LogicOptions_GetShowCrewNames, 1, enabled);
            PatchIntReturn(RVA::PlayerControl_IsMushroomMixup, 0, enabled);
            PatchIntReturn(RVA::PlayerControl_AreCommsAffected, 0, enabled);
            break;
        case 9:
            PatchIntReturn(RVA::RoleBehaviour_get_IsImpostor, 1, enabled);
            PatchIntReturn(RVA::RoleManager_IsImpostorRole, 1, enabled);
            break;
        case 10:
            PatchIntReturn(RVA::Ladder_IsCoolingDown, 0, enabled);
            PatchFloatReturn(RVA::Ladder_get_CoolDown, 0.0f, enabled);
            break;
        case 11:
            PatchIntReturn(RVA::LogicOptions_GetGhostsDoTasks, 1, enabled);
            PatchIntReturn(RVA::PlayerControl_AllTasksCompleted, 1, enabled);
            break;
        case 12:
            PatchIntReturn(RVA::LogicOptions_GetConfirmImpostor, 1, enabled);
            PatchIntReturn(RVA::LogicOptions_GetVisualTasks, 1, enabled);
            break;
        case 13:
            PatchIntReturn(RVA::InnerNetClient_CanBan, 1, enabled);
            PatchIntReturn(RVA::InnerNetClient_CanKick, 1, enabled);
            break;
        case 14:
            PatchIntReturn(RVA::EOSManager_IsFreechatAllowed, 1, enabled);
            PatchIntReturn(RVA::EOSManager_IsFriendsListAllowed, 1, enabled);
            PatchIntReturn(RVA::AccountMgr_CanMinorSetCustomName, 1, enabled);
            break;
        case 15:
            PatchFloatReturn(RVA::PlayerPhysics_get_TrueSpeed, value * 3.5f, enabled);
            PatchFloatReturn(RVA::PlayerPhysics_get_SpeedMod, value, enabled);
            PatchFloatReturn(RVA::LogicOptions_GetPlayerSpeedMod, value, enabled);
            break;
    }
}
