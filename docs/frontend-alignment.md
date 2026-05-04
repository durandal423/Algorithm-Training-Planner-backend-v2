# Frontend Alignment Notes

This document lists the API behavior frontend code should align with after the
background task refactor.

## Runtime Processes

Run these backend processes in development when testing async flows:

- `Algorithm-Training-Planner-backend`: HTTP API server.
- `Algorithm-task-worker`: consumes `background_tasks` for training plan
  generation and submission creation.
- `Algorithm-judge-worker`: consumes the `submissions` judge queue and runs code.

If `Algorithm-task-worker` is not running, async API responses will stay
`pending`.

## Auth Is Synchronous

`POST /api/auth/register`

- Returns `201`.
- Returns `user`, `token`, and `expiresAt` directly.
- Does not return `authTaskId`.
- Does not require task polling.

`POST /api/auth/login`

- Returns `200`.
- Returns `user`, `token`, and `expiresAt` directly.
- Does not return `authTaskId`.
- Does not require task polling.

Store the returned token and send it as:

```http
Authorization: Bearer <token>
```

## Training Plan Generation Is Async

`POST /api/training/plans/generate`

Response:

```json
{
  "trainingTaskId": "training_1",
  "status": "pending"
}
```

Poll:

```http
GET /api/tasks/<trainingTaskId>
Authorization: Bearer <token>
```

When `task.status === "succeeded"`, read:

- `task.result.plan`
- `task.result.items`

When `task.status === "failed"`, show `task.error`.

## Submission Creation Is Async

`POST /api/problems/<id>/submissions`

Response:

```json
{
  "submissionTaskId": "submission_1",
  "status": "pending"
}
```

Step 1: poll task creation:

```http
GET /api/tasks/<submissionTaskId>
Authorization: Bearer <token>
```

When `task.status === "succeeded"`, read the real submission id:

```json
{
  "task": {
    "result": {
      "id": 123,
      "status": "queued"
    }
  }
}
```

Step 2: poll judge status:

```http
GET /api/submissions/123
Authorization: Bearer <token>
```

Stop polling when `submission.status` is one of:

- `completed`
- `failed`
- `canceled`

## Task Status Values

Background task statuses:

- `pending`
- `running`
- `succeeded`
- `failed`

Judge submission statuses:

- `queued`
- `judging`
- `completed`
- `failed`
- `canceled`

## Frontend Checklist

- Remove auth task polling code.
- Treat register/login as immediate token responses.
- Keep polling for training plan generation.
- For code submission, poll task first, then poll the real submission.
- Ensure local/dev environments start both API server and `Algorithm-task-worker`.
