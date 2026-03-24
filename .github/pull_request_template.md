## 变更说明

<!-- 简述本次改动的目的 -->

## 变更类型

- [ ] 功能新增
- [ ] Bug修复
- [ ] 重构/优化
- [ ] CI/构建
- [ ] 文档

## 嵌入式检查清单

- [ ] 无动态内存分配 (malloc/free)
- [ ] 使用 stdint.h 固定宽度类型
- [ ] 寄存器访问使用 volatile
- [ ] ISR 中无阻塞操作
- [ ] 已通过本地 Unity 测试 (`cd test && make`)
- [ ] 无新增编译警告 (-Wall -Wextra -Werror)
