# HPCC to UNISON/ns-3 迁移问题报告

## 概述

本报告记录了将 HPCC（High Precision Congestion Control）模拟代码从旧版 ns-3 迁移到新版 ns-3 (3.36+) 过程中遇到的兼容性问题及修复方案。

### 关键发现：DataRate 除法运算符
**重要更正**：新版 ns-3 的 `DataRate` 类**保留了**乘法 `*`、加法 `+`、减法 `-` 运算符，但**移除了除法运算符 `/`**。这是编译错误的主要来源之一。最简单的修复方案是在代码中添加 3 行辅助函数（见 6.1 节）。

---

## 一、问题分类

### 1.1 核心 API 变更

| 问题 | 旧版本 | 新版本 | 影响文件 |
|------|--------|--------|----------|
| CalculateTxTime | `rate.CalculateTxTime(bytes)` | `rate.CalculateBitsTxTime(bits)` | rdma-hw.cc |
| RandomVariable | `random-variable.h` | `random-variable-stream.h` | rdma-client.cc |
| Simulator 头文件 | 自动包含 | 需显式包含 | switch-node.cc |

### 1.2 数据封装加强

| 问题 | 旧版本 | 新版本 | 影响文件 |
|------|--------|--------|----------|
| Node::m_id | 公有 | 私有 | switch-node.cc |
| Node::m_devices | 公有 | 私有 | switch-node.cc |
| Node::m_node_type | 公有 | 私有 | switch-node.cc |

### 1.3 类型系统变更

| 问题 | 旧版本 | 新版本 | 影响文件 |
|------|--------|--------|----------|
| DataRate 除法运算 | 支持 `operator/` | **移除** `/`，保留 `*`, `+`, `-` | rdma-hw.cc |
| Packet::GetBuffer | 存在 | 移除 | switch-node.cc |
| PppHeader::GetStaticSize | 存在 | 移除 | switch-node.cc |

---

## 二、详细问题及修复方案

### 2.1 rdma-hw.cc 修复

#### 2.1.1 CalculateTxTime 方法变更

**问题描述**: `DataRate::CalculateTxTime()` 已更名为 `CalculateBitsTxTime()`，且参数单位从字节变为比特。

**错误位置**:
- Line 594: `m_rate.CalculateTxTime(chunkSize)`
- Line 596: `rate.CalculateTxTime(chunkSize)`
- Line 602: `qp->m_rate.CalculateTxTime(chunkSize)`
- Line 603: `qp->m_rate.CalculateTxTime(chunkSize)`

**修复方案**:
```cpp
// 修复前
Time txTime = m_rate.CalculateTxTime(chunkSize);

// 修复后
Time txTime = m_rate.CalculateBitsTxTime(chunkSize * 8);
```

#### 2.1.2 DataRate 除法运算符移除

**问题描述**: 新版 ns-3 的 DataRate **移除了除法运算符 `/`**，但仍保留乘法 `*`、加法 `+` 和减法 `-`。

当前版本 DataRate 支持的运算符：
- ✅ `operator+`, `operator+=` (DataRate + DataRate)
- ✅ `operator-`, `operator-=` (DataRate - DataRate)  
- ✅ `operator*` (DataRate * double/uint64_t)
- ❌ **`operator/` 已移除！**

**错误位置**:
- Line 650: `double * DataRate` → **可以正常工作**
- Line 703: `DataRate / int` → **需要修复**
- Line 719: `DataRate / int` → **需要修复**
- Line 735: `DataRate / int` → **需要修复**
- Line 833: `DataRate / double` → **需要修复**
- Line 859: `DataRate / double` → **需要修复**
- Line 1084: `DataRate / double` → **需要修复**

**修复方案**:
```cpp
// 修复前
new_rate = qp->hp.m_curRate / max_c + m_rai;

// 修复后（使用 GetBitRate() 进行除法，再构造新 DataRate）
new_rate = DataRate((uint64_t)(qp->hp.m_curRate.GetBitRate() / max_c)) + m_rai;

// 其他除法运算的修复示例
// DataRate / int
DataRate result = DataRate(qp->m_rate.GetBitRate() / 2);

// DataRate / double  
DataRate result = DataRate((uint64_t)(rate.GetBitRate() / divisor));

// double * DataRate （这个可以正常工作，不需要修改）
// DataRate result = factor * rate;  // ✅ 支持
```

#### 2.1.3 SeqTsHeader::SetPG 不存在

**问题描述**: `SeqTsHeader` 类没有 `SetPG` 方法。

**错误位置**: Line 556

**修复方案**: 需要检查 SeqTsHeader 类的定义，可能的方法是：
```cpp
// 方案 A: 如果 PG 信息存储在其他 header 中
IntHeader ih;
ih.SetPG(pg);

// 方案 B: 如果需要在 SeqTsHeader 中添加 SetPG 方法
// 需要修改 seq-ts-header.h 和 seq-ts-header.cc
```

---

### 2.2 switch-node.cc 修复

#### 2.2.1 Node 私有成员访问

**问题描述**: `Node` 类的成员变量在新版中变为私有。

**错误位置**:
- Line 47: `m_id = switch_id`
- Line 48: `m_node_type = 1`
- Line 92, 99, 109, 133, 222: `m_devices` 访问

**修复方案**:

**方案 A - 使用公有方法**（推荐）:
```cpp
// 修复 m_id
// 旧代码
m_id = switch_id;

// 新代码
// 如果需要在 SwitchNode 中存储 switch_id，添加成员变量
class SwitchNode : public Node {
private:
    uint32_t m_switchId;
public:
    void SetSwitchId(uint32_t id) { m_switchId = id; }
    uint32_t GetSwitchId() const { return m_switchId; }
};

// 修复 m_devices
// 旧代码
m_devices[i]
m_devices.size()

// 新代码
GetDevice(i)
GetNDevices()
```

**方案 B - 添加友元声明**（如果必须访问私有成员）:
```cpp
// 在 node.h 中
class SwitchNode;  // 前向声明

class Node : public Object {
    friend class SwitchNode;  // 添加友元
    // ...
};
```

#### 2.2.2 缺少头文件

**问题描述**: `Simulator` 类未声明。

**错误位置**: Line 224, 226, 304

**修复方案**:
```cpp
// 在 switch-node.cc 文件顶部添加
#include "ns3/simulator.h"
```

#### 2.2.3 NetDevice::SwitchSend 不存在

**问题描述**: `NetDevice` 类没有 `SwitchSend` 方法。

**错误位置**: Line 133

**修复方案**:
```cpp
// 检查是否需要类型转换
Ptr<QbbNetDevice> qbbDev = DynamicCast<QbbNetDevice>(GetDevice(i));
if (qbbDev) {
    qbbDev->SwitchSend(...);
}

// 或者如果 SwitchSend 是自定义方法，需要添加到 QbbNetDevice 类中
```

#### 2.2.4 Packet::GetBuffer 不存在

**问题描述**: `Packet` 类没有 `GetBuffer` 方法。

**错误位置**: Line 219

**修复方案**:
```cpp
// 修复前
uint8_t *buf = (uint8_t *)packet->GetBuffer();

// 修复后
uint8_t *buf = new uint8_t[packet->GetSize()];
packet->CopyData(buf, packet->GetSize());
// 使用完后记得 delete[] buf;
```

#### 2.2.5 PppHeader::GetStaticSize 不存在

**问题描述**: `PppHeader` 类没有 `GetStaticSize` 静态方法。

**错误位置**: Line 220, 221

**修复方案**:
```cpp
// 修复前
uint32_t pppLen = PppHeader::GetStaticSize();

// 修复后
PppHeader pppHeader;
uint32_t pppLen = pppHeader.GetSerializedSize();
```

---

### 2.3 rdma-client.cc 修复

#### 2.3.1 RandomVariable 头文件变更

**问题描述**: `ns3/random-variable.h` 头文件已移除。

**错误位置**: Line 31

**修复方案**:
```cpp
// 修复前
#include "ns3/random-variable.h"

// 修复后
#include "ns3/random-variable-stream.h"

// 同时需要修改 RandomVariable 的使用方式
// 旧代码
UniformVariable uv;
double value = uv.GetValue();

// 新代码
Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
double value = uv->GetValue();
```

---

### 2.4 rdma-driver.cc 修复（已完成）

#### 2.4.1 AddTraceSource 参数变更

**问题描述**: `TypeId::AddTraceSource` 方法需要 4 个参数，旧代码只提供了 3 个。

**修复方案**:
```cpp
// 修复前
.AddTraceSource("QpComplete", "A qp completes.",
    MakeTraceSourceAccessor(&RdmaDriver::m_traceQpComplete))

// 修复后
.AddTraceSource("QpComplete", "A qp completes.",
    MakeTraceSourceAccessor(&RdmaDriver::m_traceQpComplete),
    "ns3::TracedCallback<ns3::Ptr<ns3::RdmaQueuePair>>")
```

---

## 三、修复优先级建议

### 高优先级（阻止编译）
1. **头文件缺失** - rdma-client.cc (`random-variable.h`), switch-node.cc (`simulator.h`)
2. **CalculateTxTime 更名** - rdma-hw.cc (`CalculateTxTime` → `CalculateBitsTxTime`)
3. **DataRate 除法运算符** - rdma-hw.cc (最简单方案：添加 3 行辅助函数)
4. **AddTraceSource 参数** - rdma-driver.cc（已完成）

### 中优先级（需要代码重构）
1. **Node 私有成员访问** - switch-node.cc (`m_id`, `m_devices`, `m_node_type`)
2. **SeqTsHeader::SetPG** - rdma-hw.cc (需要检查 header 定义)

### 低优先级（API 变更）
1. **GetStaticSize 移除** - switch-node.cc (使用 `GetSerializedSize()`)
2. **GetBuffer 移除** - switch-node.cc (使用 `CopyData()`)

---

## 四、验证步骤

修复完成后，按以下步骤验证：

```bash
# 1. 清理构建
./ns3 clean

# 2. 重新配置
./ns3 configure --enable-mtp --enable-examples

# 3. 构建项目
./ns3 build

# 4. 运行测试（如果有）
./ns3 run dctcp-example
```

---

## 五、参考资料

1. **ns-3 3.36 迁移指南**: https://www.nsnam.org/docs/release/3.36/
2. **DataRate API 变更**: 从运算符重载改为显式方法调用
3. **RandomVariable 重构**: 统一使用 RandomVariableStream 架构

---

## 六、附录：完整修复代码片段

### 6.1 DataRate 除法运算符修复（已完成）

**方案 A - 直接修改 DataRate 类（推荐，已实施）**：

在 `src/network/utils/data-rate.h` 中添加声明：
```cpp
class DataRate {
public:
    // ... 现有代码 ...
    
    /**
     * @brief Divides the DataRate by a double
     */
    DataRate operator/(double rhs) const;
    
    /**
     * @brief Divides the DataRate by an integer
     */
    DataRate operator/(int rhs) const;
    
    /**
     * @brief Divides the DataRate by a uint64_t
     */
    DataRate operator/(uint64_t rhs) const;
};
```

在 `src/network/utils/data-rate.cc` 中添加实现：
```cpp
DataRate
DataRate::operator/(double rhs) const
{
    return DataRate((uint64_t)(m_bps / rhs));
}

DataRate
DataRate::operator/(int rhs) const
{
    return DataRate(m_bps / (uint64_t)rhs);
}

DataRate
DataRate::operator/(uint64_t rhs) const
{
    return DataRate(m_bps / rhs);
}
```

**优势**：
- ✅ 无需修改任何使用 DataRate 除法的代码
- ✅ 符合 C++ 运算符重载惯例（作为成员函数）
- ✅ 与 DataRate 类中现有的 `operator*` 风格一致

**方案 B - 全局辅助函数（如果无法修改 DataRate 类）**：

如果无法修改 DataRate 类，可以在 `rdma-hw.cc` 顶部添加：
```cpp
inline DataRate operator/(const DataRate& rate, double divisor) {
    return DataRate((uint64_t)(rate.GetBitRate() / divisor));
}
inline DataRate operator/(const DataRate& rate, int divisor) {
    return DataRate(rate.GetBitRate() / (uint64_t)divisor);
}
inline DataRate operator/(const DataRate& rate, uint64_t divisor) {
    return DataRate(rate.GetBitRate() / divisor);
}
```

**注意**：乘法运算符 `operator*` 已经存在于 DataRate 类中（见 data-rate.h 第 147 行），不需要额外定义。

### 6.2 Node 扩展方案

```cpp
// switch-node.h
class SwitchNode : public Node {
public:
    static TypeId GetTypeId(void);
    SwitchNode();
    
    void SetSwitchId(uint32_t id);
    uint32_t GetSwitchId() const;
    
    // 其他方法...
    
private:
    uint32_t m_switchId;
};

// switch-node.cc
void SwitchNode::SetSwitchId(uint32_t id) {
    m_switchId = id;
}

uint32_t SwitchNode::GetSwitchId() const {
    return m_switchId;
}
```

---

*报告生成日期: 2024年*
*适用版本: ns-3.36+ / UNISON-for-ns-3*
