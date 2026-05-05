# API 文档

本文档面向前端对接。默认服务地址为 `http://localhost:8080`，所有业务接口都返回 JSON。

前端迁移重点请先看 `docs/frontend-alignment.md`，其中汇总了注册/登录同步响应、训练计划任务轮询、提交创建任务轮询等当前行为。

## 通用约定

### 认证

登录或注册成功后会返回 `token`。需要登录的接口添加请求头：

```http
Authorization: Bearer <token>
```

管理员接口要求当前用户 `role = "admin"`。

### 错误响应

常见错误响应格式：

```json
{
  "error": "authentication required"
}
```

常见状态码：

- `400`：请求体字段缺失、类型错误、业务校验失败。
- `401`：未登录或 token 无效。
- `403`：已登录但权限不足。
- `404`：资源不存在或无权访问该私有资源。

### 后台任务轮询

训练计划生成和提交创建会返回 `202` 与任务引用，例如 `trainingTaskId` 或 `submissionTaskId`。前端使用：

```http
GET /api/tasks/<taskRef>
Authorization: Bearer <token>
```

轮询任务状态。任务状态为：

- `pending`
- `running`
- `succeeded`
- `failed`

`succeeded` 时读取 `task.result`；`failed` 时显示 `task.error`。注册和登录不走后台任务，直接同步返回 `user`、`token`、`expiresAt`。

### 枚举

用户角色：

- `user`
- `admin`

用户状态：

- `active`
- `disabled`

题目可见性：

- `public`
- `hidden`

提交状态：

- `queued`
- `judging`
- `completed`
- `failed`
- `canceled`

判题 verdict：

- `AC`
- `WA`
- `TLE`
- `MLE`
- `RE`
- `CE`
- `OLE`
- `JE`

测试点可见性：

- `sample`
- `hidden`
- `admin_only`

输出比较模式：

- `exact`
- `trim_trailing`
- `ignore_whitespace`
- `float_epsilon`

训练计划状态：

- `draft`
- `active`
- `completed`
- `archived`

训练计划项状态：

- `not_started`
- `failed`
- `completed`

## 数据模型

### User

```json
{
  "id": 1,
  "username": "alice",
  "email": "alice@example.com",
  "displayName": "Alice",
  "role": "user",
  "status": "active"
}
```

### Tag

```json
{
  "id": 1,
  "name": "动态规划",
  "description": ""
}
```

### Problem

题目详情使用完整 `Problem`。题目列表使用下面的 `ProblemSummary`，不会返回正文、输入输出格式、样例和提示等大文本字段。

```json
{
  "id": 1,
  "code": "P12134",
  "title": "画展布置",
  "sourcePlatform": "local",
  "sourceUrl": "https://example.com/problem/P12134",
  "difficulty": 1300,
  "estimatedMinutes": 25,
  "description": "",
  "inputFormat": "",
  "outputFormat": "",
  "sampleInput": "4 2\n1 5 2 4\n",
  "sampleOutput": "3\n",
  "hint": "",
  "visibility": "public",
  "createdBy": 1,
  "createdAt": "2026-05-01 13:00:00+00",
  "updatedAt": "2026-05-01 13:00:00+00",
  "tags": [
    {
      "id": 1,
      "name": "滑动窗口",
      "description": ""
    }
  ]
}
```

### ProblemSummary

`GET /api/problems` 的列表项。

```json
{
  "id": 1,
  "code": "P12134",
  "title": "画展布置",
  "sourcePlatform": "local",
  "sourceUrl": "https://example.com/problem/P12134",
  "difficulty": 1300,
  "estimatedMinutes": 25,
  "visibility": "public",
  "createdBy": 1,
  "createdAt": "2026-05-01 13:00:00+00",
  "updatedAt": "2026-05-01 13:00:00+00",
  "tags": [
    {
      "id": 1,
      "name": "滑动窗口",
      "description": ""
    }
  ]
}
```

### SubmissionSummary

`GET /api/submissions` 和 `GET /api/admin/submissions` 的列表项。不返回 `sourceCode`，也不返回 `results`。

```json
{
  "id": 100,
  "problemId": 1,
  "userId": 2,
  "language": "cpp",
  "status": "completed",
  "verdict": "AC",
  "score": 40,
  "compileStdout": "",
  "compileStderr": "",
  "compileTimeMs": 435,
  "maxTimeMs": 20,
  "maxMemoryKb": 0,
  "submittedAt": "2026-05-01 13:00:00+00",
  "startedAt": "2026-05-01 13:00:01+00",
  "finishedAt": "2026-05-01 13:00:02+00"
}
```

### Submission

提交详情使用完整 `Submission`，包含 `sourceCode` 和 `results`。

```json
{
  "id": 100,
  "problemId": 1,
  "userId": 2,
  "language": "cpp",
  "status": "completed",
  "verdict": "AC",
  "score": 40,
  "compileStdout": "",
  "compileStderr": "",
  "compileTimeMs": 435,
  "maxTimeMs": 20,
  "maxMemoryKb": 0,
  "submittedAt": "2026-05-01 13:00:00+00",
  "startedAt": "2026-05-01 13:00:01+00",
  "finishedAt": "2026-05-01 13:00:02+00",
  "sourceCode": "#include <iostream>\nint main(){return 0;}\n",
  "results": []
}
```

### SubmissionResult

```json
{
  "id": 1,
  "submissionId": 100,
  "testCaseId": 10,
  "orderIndex": 1,
  "status": "completed",
  "verdict": "AC",
  "timeMs": 12,
  "memoryKb": 0,
  "exitCode": 0,
  "stdoutSample": "3\n",
  "stderrSample": "",
  "message": "accepted"
}
```

### JudgeConfig

```json
{
  "id": 1,
  "problemId": 1,
  "enabled": true,
  "language": "cpp",
  "compileCommandTemplate": null,
  "runCommandTemplate": null,
  "timeLimitMs": 1000,
  "memoryLimitMb": 256,
  "outputLimitKb": 1024,
  "compareMode": "ignore_whitespace",
  "floatEpsilon": 0.000001
}
```

### JudgeTestCase

```json
{
  "id": 1,
  "problemId": 1,
  "name": "sample",
  "inputData": "4 2\n1 5 2 4\n",
  "expectedOutput": "3\n",
  "visibility": "sample",
  "points": 10,
  "orderIndex": 1,
  "timeLimitMs": null,
  "memoryLimitMb": null
}
```

### TrainingGoal

```json
{
  "id": 1,
  "userId": 2,
  "name": "蓝桥杯训练",
  "targetCount": 10,
  "timeBudgetMinutes": 180,
  "difficultyMin": 1000,
  "difficultyMax": 1800,
  "preferWrongProblems": true,
  "preferWeakTags": true,
  "difficultyWeight": 10,
  "tagCoverageWeight": 20,
  "wrongProblemWeight": 15,
  "weakTagWeight": 15,
  "estimatedTimeWeight": 1,
  "createdAt": "2026-05-01 13:00:00+00",
  "tags": ["动态规划", "滑动窗口"]
}
```

### TrainingPlan

```json
{
  "id": 1,
  "userId": 2,
  "goalId": 1,
  "name": "蓝桥杯训练 plan",
  "status": "draft",
  "candidateCount": 20,
  "selectedCount": 8,
  "totalEstimatedMinutes": 175,
  "algorithmSummary": {
    "algorithm": "dp",
    "totalElapsedMicroseconds": 18432,
    "candidateWindow": {
      "leftIndex": 1,
      "rightIndex": 12,
      "leftDifficulty": 1000,
      "rightDifficulty": 1700,
      "difficultySpan": 700,
      "candidateCount": 12,
      "wrongProblemCount": 2,
      "weakTagHitCount": 5,
      "coverageRatio": 0.75,
      "coveredTags": ["动态规划", "滑动窗口"],
      "uncoveredTags": ["图论"],
      "elapsedMicroseconds": 3210
    },
    "optimizationResult": {
      "selectedCount": 8,
      "totalEstimatedMinutes": 175,
      "totalScore": 574,
      "coveredTagMask": 7,
      "coveredTags": ["动态规划", "滑动窗口"],
      "uncoveredTags": ["图论"],
      "elapsedMicroseconds": 15222
    }
  },
  "createdAt": "2026-05-01 13:00:00+00"
}
```

### TrainingPlanItem

```json
{
  "id": 1,
  "planId": 1,
  "problemId": 10,
  "orderIndex": 1,
  "estimatedMinutes": 25,
  "score": 234,
  "selectedReason": "双指针稳定窗口候选；DP最优转移选中；覆盖目标标签=双指针,滑动窗口；score=145",
  "tags": ["双指针", "滑动窗口"],
  "status": "not_started",
  "lastSubmissionId": null,
  "lastVerdict": null,
  "updatedAt": "2026-05-01 13:00:00+00"
}
```

## Auth API

### 注册

`POST /api/auth/register`

请求体：

```json
{
  "username": "alice",
  "email": "alice@example.com",
  "displayName": "Alice",
  "password": "password-123"
}
```

字段：

- `username` 必填，3 到 64 字符，只允许字母、数字、下划线、连字符。
- `email` 可选。
- `displayName` 可选；为空时后端使用 `username`。
- `password` 必填，8 到 256 字符。

响应 `201`：

```json
{
  "user": {
    "id": 1,
    "username": "alice",
    "email": "alice@example.com",
    "displayName": "Alice",
    "role": "user",
    "status": "active",
    "createdAt": "2026-05-01 13:00:00+00"
  },
  "token": "session-token",
  "expiresAt": "2026-05-15 13:00:00+00"
}
```

### 登录

`POST /api/auth/login`

请求体：

```json
{
  "identifier": "alice",
  "password": "password-123"
}
```

`identifier` 可以是用户名或邮箱。

响应 `200`：

```json
{
  "user": {
    "id": 1,
    "username": "alice",
    "email": "alice@example.com",
    "displayName": "Alice",
    "role": "user",
    "status": "active",
    "createdAt": "2026-05-01 13:00:00+00"
  },
  "token": "session-token",
  "expiresAt": "2026-05-15 13:00:00+00"
}
```

### 当前用户

`GET /api/auth/me`

权限：登录用户。

响应 `200`：

```json
{
  "user": {}
}
```

### 登出

`POST /api/auth/logout`

响应 `200`：

```json
{
  "revoked": true
}
```

## Problem API

### 题目列表

`GET /api/problems`

权限：公开。管理员登录时可看到 hidden 题目。

查询参数：

- `q`：按 `code` 或 `title` 模糊搜索。
- `tag`：按标签名过滤。
- `limit`：默认 `50`，后端限制 `1..200`。
- `offset`：默认 `0`。

示例：

```http
GET /api/problems?q=P12134&tag=滑动窗口&limit=20&offset=0
```

响应 `200`：

```json
{
  "items": [
    {
      "id": 1,
      "code": "P12134",
      "title": "画展布置",
      "sourcePlatform": "local",
      "sourceUrl": "https://example.com/problem/P12134",
      "difficulty": 1300,
      "estimatedMinutes": 25,
      "visibility": "public",
      "createdBy": 1,
      "createdAt": "2026-05-01 13:00:00+00",
      "updatedAt": "2026-05-01 13:00:00+00",
      "tags": [
        {
          "id": 1,
          "name": "滑动窗口",
          "description": ""
        }
      ]
    }
  ]
}
```

说明：列表项为 `ProblemSummary`，不包含 `description`、`inputFormat`、`outputFormat`、`sampleInput`、`sampleOutput`、`hint`。

### 题目数量

`GET /api/problems/count`

权限：公开。管理员登录时会统计 hidden 题目；未登录或普通用户只统计 public 题目。

响应 `200`：

```json
{
  "count": 1000002
}
```

### 题目详情

`GET /api/problems/<id>`

权限：公开。hidden 题目仅管理员可见。

响应 `200`：

```json
{
  "problem": {
    "id": 1,
    "code": "P12134",
    "title": "画展布置",
    "sourcePlatform": "local",
    "sourceUrl": "https://example.com/problem/P12134",
    "difficulty": 1300,
    "estimatedMinutes": 25,
    "description": "",
    "inputFormat": "",
    "outputFormat": "",
    "sampleInput": "4 2\n1 5 2 4\n",
    "sampleOutput": "3\n",
    "hint": "",
    "visibility": "public",
    "createdBy": 1,
    "createdAt": "2026-05-01 13:00:00+00",
    "updatedAt": "2026-05-01 13:00:00+00",
    "tags": [
      {
        "id": 1,
        "name": "滑动窗口",
        "description": ""
      }
    ]
  }
}
```

### 标签列表

`GET /api/tags`

响应 `200`：

```json
{
  "items": []
}
```

### 创建题目

`POST /api/admin/problems`

权限：管理员。

请求体：

```json
{
  "code": "P12134",
  "title": "画展布置",
  "sourcePlatform": "local",
  "sourceUrl": "https://example.com/problem/P12134",
  "difficulty": 1300,
  "estimatedMinutes": 25,
  "description": "",
  "inputFormat": "",
  "outputFormat": "",
  "sampleInput": "4 2\n1 5 2 4\n",
  "sampleOutput": "3\n",
  "hint": "",
  "visibility": "public",
  "tags": ["排序", "滑动窗口"]
}
```

必填字段：

- `code`
- `title`
- `difficulty`

响应 `201`：

```json
{
  "id": 1
}
```

### 更新题目

`PUT /api/admin/problems/<id>`

权限：管理员。

请求体同创建题目。当前实现是整体更新，前端应提交完整表单。

响应 `200`：

```json
{
  "updated": true
}
```

### 删除题目

`DELETE /api/admin/problems/<id>`

权限：管理员。

响应 `200`：

```json
{
  "deleted": true
}
```

## Submission API

### 我的提交列表

`GET /api/submissions`

权限：登录用户。

查询参数：

- `problemId`：可选。
- `status`：可选。
- `verdict`：可选。
- `limit`：默认 `50`，后端限制 `1..200`。
- `offset`：默认 `0`。

说明：普通用户只能看到自己的提交；即使传 `userId` 也会被后端覆盖为当前用户。

响应 `200`：

```json
{
  "items": [
    {
      "id": 100,
      "problemId": 1,
      "userId": 2,
      "language": "cpp",
      "status": "completed",
      "verdict": "AC",
      "score": 40,
      "compileStdout": "",
      "compileStderr": "",
      "compileTimeMs": 435,
      "maxTimeMs": 20,
      "maxMemoryKb": 0,
      "submittedAt": "2026-05-01 13:00:00+00",
      "startedAt": "2026-05-01 13:00:01+00",
      "finishedAt": "2026-05-01 13:00:02+00"
    }
  ]
}
```

说明：列表项为 `SubmissionSummary`，不包含 `sourceCode` 和 `results`。

### 提交代码

`POST /api/problems/<id>/submissions`

权限：登录用户。

请求体：

```json
{
  "language": "cpp",
  "sourceCode": "#include <iostream>\nint main(){std::cout << 1;}\n"
}
```

字段：

- `language` 可选，当前仅支持 `cpp`。
- `sourceCode` 必填，1 到 262144 bytes。

响应 `202`：

```json
{
  "submissionTaskId": "submission_100",
  "status": "pending"
}
```

提交创建结果通过 `GET /api/tasks/<submissionTaskId>` 查询。任务成功后 `result.id` 是真实提交 id，随后再用 `GET /api/submissions/<id>` 轮询判题状态，直到 `status` 为 `completed`、`failed` 或 `canceled`。

### 提交详情

`GET /api/submissions/<id>`

权限：登录用户。普通用户只能查看自己的提交，管理员可查看任意提交。

响应 `200`：

```json
{
  "submission": {
    "id": 100,
    "problemId": 1,
    "userId": 2,
    "language": "cpp",
    "status": "completed",
    "verdict": "AC",
    "score": 40,
    "compileStdout": "",
    "compileStderr": "",
    "compileTimeMs": 435,
    "maxTimeMs": 20,
    "maxMemoryKb": 0,
    "submittedAt": "2026-05-01 13:00:00+00",
    "startedAt": "2026-05-01 13:00:01+00",
    "finishedAt": "2026-05-01 13:00:02+00",
    "sourceCode": "#include <iostream>\nint main(){return 0;}\n",
    "results": []
  }
}
```

### 管理员提交列表

`GET /api/admin/submissions`

权限：管理员。

查询参数：

- `problemId`：可选。
- `userId`：可选。
- `status`：可选。
- `verdict`：可选。
- `limit`：默认 `50`，后端限制 `1..200`。
- `offset`：默认 `0`。

响应 `200`：

```json
{
  "items": [
    {
      "id": 100,
      "problemId": 1,
      "userId": 2,
      "language": "cpp",
      "status": "completed",
      "verdict": "AC",
      "score": 40,
      "compileStdout": "",
      "compileStderr": "",
      "compileTimeMs": 435,
      "maxTimeMs": 20,
      "maxMemoryKb": 0,
      "submittedAt": "2026-05-01 13:00:00+00",
      "startedAt": "2026-05-01 13:00:01+00",
      "finishedAt": "2026-05-01 13:00:02+00"
    }
  ]
}
```

说明：列表项为 `SubmissionSummary`，不包含 `sourceCode` 和 `results`。

### 重新判题

`POST /api/admin/submissions/<id>/rejudge`

权限：管理员。

行为：

- 删除旧 `submission_results`。
- 将 submission 重新置为 `queued`。
- Judge 线程会再次领取执行。

响应 `200`：

```json
{
  "queued": true
}
```

## Judge Admin API

### 获取判题配置

`GET /api/admin/problems/<id>/judge-config`

权限：管理员。

响应 `200`：

```json
{
  "config": {}
}
```

### 保存判题配置

`PUT /api/admin/problems/<id>/judge-config`

权限：管理员。

请求体：

```json
{
  "enabled": true,
  "language": "cpp",
  "compileCommandTemplate": null,
  "runCommandTemplate": null,
  "timeLimitMs": 1000,
  "memoryLimitMb": 256,
  "outputLimitKb": 1024,
  "compareMode": "ignore_whitespace",
  "floatEpsilon": 0.000001
}
```

默认模板：

- 编译：`g++ -std=c++17 -O2 -pipe {source} -o {executable}`
- 运行：`{executable} < {input} > {stdout} 2> {stderr}`

响应 `200`：

```json
{
  "config": {}
}
```

### 测试点列表

`GET /api/admin/problems/<id>/judge-test-cases`

权限：管理员。

响应 `200`：

```json
{
  "items": []
}
```

### 创建测试点

`POST /api/admin/problems/<id>/judge-test-cases`

权限：管理员。

请求体：

```json
{
  "name": "sample",
  "inputData": "4 2\n1 5 2 4\n",
  "expectedOutput": "3\n",
  "visibility": "sample",
  "points": 10,
  "orderIndex": 1,
  "timeLimitMs": null,
  "memoryLimitMb": null
}
```

必填字段：

- `inputData`

响应 `201`：

```json
{
  "id": 1
}
```

### 更新测试点

`PUT /api/admin/judge-test-cases/<id>`

权限：管理员。

请求体同创建测试点。当前实现是整体更新，前端应提交完整表单。

响应 `200`：

```json
{
  "testCase": {}
}
```

### 删除测试点

`DELETE /api/admin/judge-test-cases/<id>`

权限：管理员。

响应 `200`：

```json
{
  "deleted": true
}
```

## Training API

### 训练目标列表

`GET /api/training/goals`

权限：登录用户。

响应 `200`：

```json
{
  "items": []
}
```

### 创建训练目标

`POST /api/training/goals`

权限：登录用户。

请求体：

```json
{
  "name": "蓝桥杯训练",
  "targetCount": 10,
  "timeBudgetMinutes": 180,
  "difficultyMin": 1000,
  "difficultyMax": 1800,
  "preferWrongProblems": true,
  "preferWeakTags": true,
  "difficultyWeight": 10,
  "tagCoverageWeight": 20,
  "wrongProblemWeight": 15,
  "weakTagWeight": 15,
  "estimatedTimeWeight": 1,
  "tags": ["动态规划", "滑动窗口"]
}
```

必填字段：

- `name`

响应 `201`：

```json
{
  "goal": {}
}
```

### 训练目标详情

`GET /api/training/goals/<id>`

权限：登录用户，只能访问自己的目标。

响应 `200`：

```json
{
  "goal": {}
}
```

### 更新训练目标

`PUT /api/training/goals/<id>`

权限：登录用户，只能更新自己的目标。

请求体同创建训练目标。当前实现是整体更新，前端应提交完整表单。

响应 `200`：

```json
{
  "goal": {}
}
```

### 删除训练目标

`DELETE /api/training/goals/<id>`

权限：登录用户，只能删除自己的目标。

响应 `200`：

```json
{
  "deleted": true
}
```

### 生成训练计划

`POST /api/training/plans/generate`

权限：登录用户。

请求体：

```json
{
  "goalId": 1,
  "name": "第 1 周训练",
  "status": "draft"
}
```

字段：

- `goalId` 必填。
- `name` 可选，默认 `<goal.name> plan`。
- `status` 可选，默认 `draft`。

响应 `202`：

该接口会创建后台任务并立即返回，避免训练计划生成占用 Crow 工作线程。后台任务会先调用 `selectStableDifficultyWindow` 用双指针/滑动窗口从候选题中选出稳定难度区间，再调用 `optimizeTrainingPlanByDP` 在 `targetCount` 和 `timeBudgetMinutes` 约束下做 0/1 背包式动态规划选题。

```json
{
  "trainingTaskId": "training_1",
  "status": "pending"
}
```

生成结果通过 `GET /api/tasks/<trainingTaskId>` 查询。

任务成功后，`task.result.plan.algorithmSummary` 会返回结构化耗时与选题摘要；`task.result.items[*].tags` 会返回所选题目的标签数组。

### 生成训练计划（暴力对照组）

`POST /api/training/plans/generate/bruteforce`

权限：登录用户。

请求体同 `POST /api/training/plans/generate`。

响应 `202`：

该接口会创建后台任务并立即返回。后台任务使用无组合数量上限的对照组搜索生成计划，用于和默认的稳定难度窗口 + DP 算法做演示对比。

```json
{
  "trainingTaskId": "training_2",
  "status": "pending",
  "algorithm": "bruteforce"
}
```

暴力对照组完成后，`algorithmSummary` 的结构与默认算法一致，差异是 `algorithm` 为 `bruteforce`，`candidateWindow` 为 `null`，`optimizationResult` 会额外包含 `combinationsEvaluated` 和 `combinationLimit`；当前无组合数量上限时 `combinationLimit` 为 `null`。

### 查询后台任务

`GET /api/tasks/<taskRef>`

权限：登录用户只能查询自己的训练/提交任务；管理员可查询所有登录态任务。

响应 `200`：

```json
{
  "task": {
    "id": 1,
    "taskRef": "training_1",
    "trainingTaskId": "training_1",
    "type": "training.plan.generate",
    "status": "succeeded",
    "result": {
      "trainingTaskId": "training_1",
      "plan": {
        "id": 12,
        "name": "第 1 周训练",
        "status": "draft",
        "candidateCount": 20,
        "selectedCount": 8,
        "totalEstimatedMinutes": 175,
        "algorithmSummary": {
          "algorithm": "dp",
          "totalElapsedMicroseconds": 18432,
          "candidateWindow": {
            "leftIndex": 1,
            "rightIndex": 12,
            "leftDifficulty": 1000,
            "rightDifficulty": 1700,
            "difficultySpan": 700,
            "candidateCount": 12,
            "wrongProblemCount": 2,
            "weakTagHitCount": 5,
            "coverageRatio": 0.75,
            "coveredTags": ["动态规划", "滑动窗口"],
            "uncoveredTags": ["图论"],
            "elapsedMicroseconds": 3210
          },
          "optimizationResult": {
            "selectedCount": 8,
            "totalEstimatedMinutes": 175,
            "totalScore": 574,
            "coveredTagMask": 7,
            "coveredTags": ["动态规划", "滑动窗口"],
            "uncoveredTags": ["图论"],
            "elapsedMicroseconds": 15222
          }
        },
        "createdAt": "2026-05-03 06:18:18.342557+00"
      },
      "items": [
        {
          "id": 1,
          "planId": 12,
          "problemId": 10,
          "orderIndex": 1,
          "estimatedMinutes": 25,
          "score": 234,
          "selectedReason": "双指针稳定窗口候选；DP最优转移选中；覆盖目标标签=双指针,滑动窗口；score=145",
          "tags": ["双指针", "滑动窗口"],
          "status": "not_started",
          "lastSubmissionId": null,
          "lastVerdict": null,
          "updatedAt": "2026-05-03 06:18:18.342557+00"
        }
      ]
    },
    "error": null,
    "createdAt": "2026-05-03 06:18:18.267515+00",
    "startedAt": "2026-05-03 06:18:18.279451+00",
    "finishedAt": "2026-05-03 06:18:18.342557+00"
  }
}
```

### 训练计划列表

`GET /api/training/plans`

权限：登录用户。

响应 `200`：

```json
{
  "items": []
}
```

### 训练计划详情

`GET /api/training/plans/<id>`

权限：登录用户，只能查看自己的计划。

响应 `200`：

```json
{
  "plan": {
    "id": 12,
    "name": "第 1 周训练",
    "status": "draft",
    "candidateCount": 20,
    "selectedCount": 8,
    "totalEstimatedMinutes": 175,
    "algorithmSummary": {
      "algorithm": "dp",
      "totalElapsedMicroseconds": 18432,
      "candidateWindow": {
        "leftIndex": 1,
        "rightIndex": 12,
        "leftDifficulty": 1000,
        "rightDifficulty": 1700,
        "difficultySpan": 700,
        "candidateCount": 12,
        "wrongProblemCount": 2,
        "weakTagHitCount": 5,
        "coverageRatio": 0.75,
        "coveredTags": ["动态规划", "滑动窗口"],
        "uncoveredTags": ["图论"],
        "elapsedMicroseconds": 3210
      },
      "optimizationResult": {
        "selectedCount": 8,
        "totalEstimatedMinutes": 175,
        "totalScore": 574,
        "coveredTagMask": 7,
        "coveredTags": ["动态规划", "滑动窗口"],
        "uncoveredTags": ["图论"],
        "elapsedMicroseconds": 15222
      }
    },
    "createdAt": "2026-05-03 06:18:18.342557+00"
  },
  "items": [
    {
      "id": 1,
      "planId": 12,
      "problemId": 10,
      "orderIndex": 1,
      "estimatedMinutes": 25,
      "score": 234,
      "selectedReason": "双指针稳定窗口候选；DP最优转移选中；覆盖目标标签=双指针,滑动窗口；score=145",
      "tags": ["双指针", "滑动窗口"],
      "status": "not_started",
      "lastSubmissionId": null,
      "lastVerdict": null,
      "updatedAt": "2026-05-03 06:18:18.342557+00"
    }
  ]
}
```

### 训练计划项列表

`GET /api/training/plans/<id>/items`

权限：登录用户，只能查看自己的计划。

响应 `200`：

```json
{
  "items": [
    {
      "id": 1,
      "planId": 12,
      "problemId": 10,
      "orderIndex": 1,
      "estimatedMinutes": 25,
      "score": 234,
      "selectedReason": "双指针稳定窗口候选；DP最优转移选中；覆盖目标标签=双指针,滑动窗口；score=145",
      "tags": ["双指针", "滑动窗口"],
      "status": "not_started",
      "lastSubmissionId": null,
      "lastVerdict": null,
      "updatedAt": "2026-05-03 06:18:18.342557+00"
    }
  ]
}
```

## Admin API

### 审计日志

`GET /api/admin/audit-logs`

权限：管理员。

查询参数：

- `limit`：默认 `100`，后端限制 `1..200`。

响应 `200`：

```json
{
  "items": [
    {
      "id": 1,
      "actorUserId": 1,
      "action": "action-name",
      "targetType": "problem",
      "targetId": 10,
      "metadata": {},
      "createdAt": "2026-05-01 13:00:00+00"
    }
  ]
}
```

## 前端接入建议

- token 存储后统一注入 `Authorization` header。
- 提交代码后进入提交详情页，按 1 到 2 秒间隔轮询 `GET /api/submissions/<id>`。
- 判题状态为 `queued` / `judging` 时展示等待状态。
- `completed` 时展示 `verdict`、`score`、编译输出、测试点结果。
- `failed` 且 `verdict = JE` 时展示系统错误提示，必要时让管理员查看测试点和 runner 日志。
- 管理员维护测试点时，建议提供“样例 / 隐藏 / 管理员可见”切换。
- 所有 PUT 接口当前按完整表单更新处理，前端不要只提交局部字段。
