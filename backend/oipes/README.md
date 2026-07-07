# OIPES - 开放生态服务

**O**pen **I**nternet **P**ublic **E**cosystem **S**ervices

## 架构

```
┌─────────────────────┐         ┌─────────────────────┐
│   客户端 (SED)       │         │   服务端            │
│   - 加密连接         │  <--->  │   - 身份认证        │
│   - 令牌存储         │  HTTPS  │   - AI 模型推理     │
│   - 推送过滤         │         │   - 推送网关        │
│   - 本地预处理       │         │   - 云存储中继      │
└─────────────────────┘         └─────────────────────┘
```

## 设计原则

- **服务端客户端解耦**：服务端由中立非营利组织运营
- **开源免绑厂**：不强制接入特定云服务
- **用户选择权**：可随时关闭或替换为兼容第三方服务端点

## API 规范

### 身份认证

```
POST /api/v1/auth/token
Content-Type: application/json

{
  "device_id": "...",
  "hardware_fingerprint": "...",
  "chain_signature": "..."
}
```

### AI 模型推理

```
POST /api/v1/ai/inference
Authorization: Bearer <token>

{
  "model": "base-model-v1",
  "prompt": "...",
  "max_tokens": 1024
}
```

### 推送通知

```
POST /api/v1/push/send
Authorization: Bearer <token>

{
  "target_device": "...",
  "priority": "high",
  "payload": {...}
}
```

## 文件结构

```
backend/oipes/
├── README.md
├── client.c           # OIPES 客户端实现
├── crypto.c           # 加密通信
├── token_store.c      # 令牌管理
└── api_spec.md        # API 规范详情
```

## 开发状态

⏳ 待实现

---

**ServEcosys — 为用户而生，因开源而活。**
