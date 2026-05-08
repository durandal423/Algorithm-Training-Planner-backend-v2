# Judge Docker Image

The judge pool uses a project-owned Docker image so every sandbox container has
GNU `/usr/bin/time` available for peak RSS collection.

Default image:

```sh
atp-judge-gcc13-time:latest
```

Build the default image:

```sh
./scripts/build_judge_image.sh
```

Build a custom image and use it through the existing override:

```sh
./scripts/build_judge_image.sh my-judge-image:latest
export ATP_JUDGE_DOCKER_IMAGE=my-judge-image:latest
```

After changing the image, remove existing pool containers so the next
`Algorithm-judge-worker` start recreates them with the current image:

```sh
containers=$(docker ps -aq --filter 'name=^/atp-judge-sandbox-[0-9]+$')
[ -z "$containers" ] || docker rm -f $containers
cmake --build build --target Algorithm-judge-worker -j 4
./build/Algorithm-judge-worker
```

Verify each recreated container:

```sh
for name in $(docker ps -a --filter 'name=^/atp-judge-sandbox-[0-9]+$' --format '{{.Names}}' | sort); do
    docker exec "$name" sh -lc 'command -v /usr/bin/time'
    docker exec "$name" sh -lc '/usr/bin/time -f "OJ_MAX_RSS_KB=%M" -o /tmp/time-test.txt sh -lc "true"'
    docker exec "$name" sh -lc 'cat /tmp/time-test.txt'
done
```
