WITH tag_seed(name, description) AS (
    VALUES
        ('双指针', '使用左右指针维护区间、配对关系或单调推进。'),
        ('滑动窗口', '在连续区间中用窗口扩张和收缩维护约束。'),
        ('动态规划', '通过状态设计、转移方程和最优子结构求解。'),
        ('贪心', '局部最优选择与交换论证。'),
        ('图论', '图建模、最短路、拓扑、连通分量等。'),
        ('搜索', 'DFS、BFS、回溯与剪枝。'),
        ('数论', '质数、同余、组合计数和模运算。'),
        ('数据结构', '堆、并查集、树状数组、线段树等维护结构。'),
        ('状态压缩', '用 bitmask 压缩集合或访问状态。'),
        ('排序', '排序与有序性维护。'),
        ('哈希表', '哈希映射与集合统计。'),
        ('连通性', '连通块、可达性与图结构关系。'),
        ('前缀和', '区间和、差分与快速查询。'),
        ('二分', '单调性判断与答案搜索。')
)
INSERT INTO tags(name, description)
SELECT name, description
FROM tag_seed
ON CONFLICT (name) DO UPDATE SET description = EXCLUDED.description;

WITH problem_seed(
    code,
    title,
    source_platform,
    source_url,
    difficulty,
    estimated_minutes,
    description,
    input_format,
    output_format,
    sample_input,
    sample_output
) AS (
    VALUES
        ('P12134', '画展布置', '蓝桥杯', 'https://www.luogu.com.cn/problem/P12134', 1300, 25,
         '从排序后的画作价值中用滑动窗口选择稳定区间，是双指针/滑动窗口展示题。',
         'N M 后接 N 个艺术价值。',
         '输出最小平方差跨度。',
         '4 2
1 5 2 4
', '3
'),
        ('P12135', '水质检测', '蓝桥杯', 'https://www.luogu.com.cn/problem/P12135', 1700, 40,
         '围绕连通性和状态设计的动态规划/状态压缩展示题。',
         '输入两行等长水质检测状态。',
         '输出最小处理代价。',
         '.##.....#
.#.#.#...
', '5
'),
        ('T0001', '区间最大连续和', '洛谷', 'https://example.com/problems/T0001', 1200, 25, '经典子段和状态转移训练。', NULL, NULL, NULL, NULL),
        ('T0002', '数组去重统计', 'LeetCode', 'https://example.com/problems/T0002', 900, 15, '哈希集合与去重计数。', NULL, NULL, NULL, NULL),
        ('T0003', '最短路径模板', 'Codeforces', 'https://example.com/problems/T0003', 1500, 35, 'Dijkstra 模板与边权建模。', NULL, NULL, NULL, NULL),
        ('T0004', '有序数组配对', '洛谷', 'https://example.com/problems/T0004', 1100, 20, '双指针完成两数配对。', NULL, NULL, NULL, NULL),
        ('T0005', '背包训练计划', '蓝桥杯', 'https://example.com/problems/T0005', 1600, 35, '01 背包与选择恢复。', NULL, NULL, NULL, NULL),
        ('T0006', '会议活动安排', '洛谷', 'https://example.com/problems/T0006', 1000, 18, '区间贪心选择。', NULL, NULL, NULL, NULL),
        ('T0007', '岛屿数量统计', 'LeetCode', 'https://example.com/problems/T0007', 1200, 25, '网格 BFS 与连通块统计。', NULL, NULL, NULL, NULL),
        ('T0008', '质数筛选器', '蓝桥杯', 'https://example.com/problems/T0008', 1000, 20, '埃氏筛与复杂度分析。', NULL, NULL, NULL, NULL),
        ('T0009', '状态压缩旅行', 'Codeforces', 'https://example.com/problems/T0009', 1900, 50, '位压缩 TSP 入门。', NULL, NULL, NULL, NULL),
        ('T0010', '窗口内最大值', '洛谷', 'https://example.com/problems/T0010', 1400, 30, '滑动窗口和单调队列思想。', NULL, NULL, NULL, NULL),
        ('T0011', '课程学习顺序', 'LeetCode', 'https://example.com/problems/T0011', 1450, 30, '拓扑排序与环检测。', NULL, NULL, NULL, NULL),
        ('T0012', '树状数组求和', '洛谷', 'https://example.com/problems/T0012', 1350, 30, '动态前缀和维护。', NULL, NULL, NULL, NULL),
        ('T0013', '最长上升子序列', '蓝桥杯', 'https://example.com/problems/T0013', 1500, 30, 'LIS 动态规划与二分优化。', NULL, NULL, NULL, NULL),
        ('T0014', '二分答案切木头', '洛谷', 'https://example.com/problems/T0014', 1150, 20, '答案单调性与二分判断。', NULL, NULL, NULL, NULL),
        ('T0015', '并查集朋友圈', 'LeetCode', 'https://example.com/problems/T0015', 1250, 25, '并查集维护连通性。', NULL, NULL, NULL, NULL),
        ('T0016', '前缀和矩阵', '蓝桥杯', 'https://example.com/problems/T0016', 1100, 22, '二维前缀和快速查询。', NULL, NULL, NULL, NULL),
        ('T0017', '最小覆盖子串', 'LeetCode', 'https://example.com/problems/T0017', 1550, 35, '滑动窗口维护字符需求。', NULL, NULL, NULL, NULL),
        ('T0018', '数位拆分计数', 'Codeforces', 'https://example.com/problems/T0018', 1650, 35, '计数 DP 与取模。', NULL, NULL, NULL, NULL),
        ('T0019', '图的连通块染色', '洛谷', 'https://example.com/problems/T0019', 1300, 28, 'DFS 染色判断连通分量。', NULL, NULL, NULL, NULL),
        ('T0020', '括号生成搜索', 'LeetCode', 'https://example.com/problems/T0020', 1050, 18, '回溯生成合法序列。', NULL, NULL, NULL, NULL),
        ('T0021', '股票买卖冷冻期', 'LeetCode', 'https://example.com/problems/T0021', 1550, 32, '多状态动态规划。', NULL, NULL, NULL, NULL),
        ('T0022', '合并区间复盘', '洛谷', 'https://example.com/problems/T0022', 950, 18, '排序后合并重叠区间。', NULL, NULL, NULL, NULL),
        ('T0023', '双端队列选择', 'Codeforces', 'https://example.com/problems/T0023', 1450, 32, '双指针与贪心判断。', NULL, NULL, NULL, NULL),
        ('T0024', '网格最短路', '蓝桥杯', 'https://example.com/problems/T0024', 1400, 30, 'BFS 最短路与障碍建模。', NULL, NULL, NULL, NULL),
        ('T0025', '子集覆盖练习', '洛谷', 'https://example.com/problems/T0025', 1750, 42, '状态压缩表示标签覆盖。', NULL, NULL, NULL, NULL),
        ('T0026', '逆序对统计', '洛谷', 'https://example.com/problems/T0026', 1500, 32, '归并排序与树状数组。', NULL, NULL, NULL, NULL),
        ('T0027', '最大不相交线段', '蓝桥杯', 'https://example.com/problems/T0027', 1350, 26, '排序加贪心选择。', NULL, NULL, NULL, NULL),
        ('T0028', '单词接龙图', 'LeetCode', 'https://example.com/problems/T0028', 1800, 45, '图建模与 BFS 搜索。', NULL, NULL, NULL, NULL),
        ('T0029', '模运算路径数', 'Codeforces', 'https://example.com/problems/T0029', 1650, 36, '数论结合 DP 计数。', NULL, NULL, NULL, NULL),
        ('T0030', '最长无重复子数组', 'LeetCode', 'https://example.com/problems/T0030', 1250, 24, '哈希表与滑动窗口维护。', NULL, NULL, NULL, NULL),
        ('T0031', '线段树区间修改', '洛谷', 'https://example.com/problems/T0031', 1850, 48, '线段树懒标记训练。', NULL, NULL, NULL, NULL),
        ('T0032', '可达状态压缩', '蓝桥杯', 'https://example.com/problems/T0032', 1750, 42, '用 bitmask 压缩访问状态。', NULL, NULL, NULL, NULL)
)
INSERT INTO problems(
    code, title, source_platform, source_url, difficulty, estimated_minutes,
    description, input_format, output_format, sample_input, sample_output, visibility
)
SELECT
    code,
    title,
    source_platform,
    source_url,
    difficulty,
    estimated_minutes,
    description,
    input_format,
    output_format,
    sample_input,
    sample_output,
    'public'
FROM problem_seed
ON CONFLICT (code) DO UPDATE SET
    title = EXCLUDED.title,
    source_platform = EXCLUDED.source_platform,
    source_url = EXCLUDED.source_url,
    difficulty = EXCLUDED.difficulty,
    estimated_minutes = EXCLUDED.estimated_minutes,
    description = EXCLUDED.description,
    input_format = EXCLUDED.input_format,
    output_format = EXCLUDED.output_format,
    sample_input = EXCLUDED.sample_input,
    sample_output = EXCLUDED.sample_output,
    visibility = EXCLUDED.visibility;

WITH mapping(problem_code, tag_name) AS (
    VALUES
        ('P12134', '排序'), ('P12134', '双指针'), ('P12134', '滑动窗口'), ('P12134', '贪心'),
        ('P12135', '动态规划'), ('P12135', '状态压缩'), ('P12135', '连通性'),
        ('T0001', '动态规划'), ('T0001', '前缀和'),
        ('T0002', '哈希表'), ('T0002', '数据结构'),
        ('T0003', '图论'), ('T0003', '数据结构'),
        ('T0004', '双指针'), ('T0004', '排序'),
        ('T0005', '动态规划'), ('T0005', '数据结构'),
        ('T0006', '贪心'), ('T0006', '排序'),
        ('T0007', '搜索'), ('T0007', '连通性'),
        ('T0008', '数论'),
        ('T0009', '动态规划'), ('T0009', '状态压缩'),
        ('T0010', '滑动窗口'), ('T0010', '数据结构'),
        ('T0011', '图论'), ('T0011', '搜索'),
        ('T0012', '数据结构'), ('T0012', '前缀和'),
        ('T0013', '动态规划'), ('T0013', '二分'),
        ('T0014', '二分'), ('T0014', '贪心'),
        ('T0015', '数据结构'), ('T0015', '连通性'),
        ('T0016', '前缀和'),
        ('T0017', '滑动窗口'), ('T0017', '双指针'), ('T0017', '哈希表'),
        ('T0018', '动态规划'), ('T0018', '数论'),
        ('T0019', '图论'), ('T0019', '搜索'), ('T0019', '连通性'),
        ('T0020', '搜索'),
        ('T0021', '动态规划'),
        ('T0022', '排序'), ('T0022', '贪心'),
        ('T0023', '双指针'), ('T0023', '贪心'),
        ('T0024', '图论'), ('T0024', '搜索'),
        ('T0025', '状态压缩'), ('T0025', '动态规划'),
        ('T0026', '排序'), ('T0026', '数据结构'),
        ('T0027', '排序'), ('T0027', '贪心'),
        ('T0028', '图论'), ('T0028', '搜索'),
        ('T0029', '数论'), ('T0029', '动态规划'),
        ('T0030', '哈希表'), ('T0030', '滑动窗口'), ('T0030', '双指针'),
        ('T0031', '数据结构'),
        ('T0032', '状态压缩'), ('T0032', '搜索')
)
INSERT INTO problem_tags(problem_id, tag_id)
SELECT p.id, t.id
FROM mapping
JOIN problems p ON p.code = mapping.problem_code
JOIN tags t ON t.name = mapping.tag_name
ON CONFLICT DO NOTHING;

-- Synthetic practice bank: keep hand-written examples above, then fill
-- T0000000033..T0001000000 with deterministic generated rows for recommendation,
-- pagination, and training-plan experiments.
WITH generated_problem_seed AS (
    SELECT
        'T' || lpad(series.n::text, 10, '0') AS code,
        vocab.title_prefixes[((series.n - 1) % array_length(vocab.title_prefixes, 1)) + 1] ||
            ' ' || lpad(series.n::text, 10, '0') AS title,
        vocab.platforms[((series.n - 1) % array_length(vocab.platforms, 1)) + 1] AS source_platform,
        'https://example.com/problems/T' || lpad(series.n::text, 10, '0') AS source_url,
        800 + ((series.n * 137) % 1501) AS difficulty,
        15 + ((series.n * 7) % 51) AS estimated_minutes,
        '自动生成的训练题，侧重' ||
            vocab.tags[((series.n - 1) % array_length(vocab.tags, 1)) + 1] ||
            '、' ||
            vocab.tags[((series.n + 3) % array_length(vocab.tags, 1)) + 1] ||
            '与基础建模。' AS description
    FROM generate_series(33, 1000000) AS series(n)
    CROSS JOIN (
        SELECT
            ARRAY[
                '区间维护训练',
                '序列统计复盘',
                '路径选择练习',
                '最优分组计划',
                '网格推演任务',
                '集合覆盖挑战',
                '子数组分析题',
                '计数方案练习',
                '树形关系整理',
                '连通块检查',
                '窗口约束判断',
                '答案搜索模板',
                '有序结构维护',
                '图上状态转移',
                '前缀信息查询',
                '组合枚举剪枝',
                '贪心交换验证',
                '哈希频次统计',
                '位集状态规划',
                '差分区间更新',
                '拓扑顺序安排',
                '最短代价求解',
                '双端选择问题',
                '模运算计数题',
                '离散化查询题',
                '多阶段决策题',
                '候选窗口筛选',
                '弱项标签强化'
            ]::text[] AS title_prefixes,
            ARRAY['洛谷', 'LeetCode', 'Codeforces', '蓝桥杯', 'AtCoder', '牛客']::text[] AS platforms,
            ARRAY[
                '双指针',
                '滑动窗口',
                '动态规划',
                '贪心',
                '图论',
                '搜索',
                '数论',
                '数据结构',
                '状态压缩',
                '排序',
                '哈希表',
                '连通性',
                '前缀和',
                '二分'
            ]::text[] AS tags
    ) AS vocab
)
INSERT INTO problems(
    code, title, source_platform, source_url, difficulty, estimated_minutes,
    description, input_format, output_format, sample_input, sample_output, visibility
)
SELECT
    code,
    title,
    source_platform,
    source_url,
    difficulty,
    estimated_minutes,
    description,
    NULL::text,
    NULL::text,
    NULL::text,
    NULL::text,
    'public'
FROM generated_problem_seed
ON CONFLICT (code) DO UPDATE SET
    title = EXCLUDED.title,
    source_platform = EXCLUDED.source_platform,
    source_url = EXCLUDED.source_url,
    difficulty = EXCLUDED.difficulty,
    estimated_minutes = EXCLUDED.estimated_minutes,
    description = EXCLUDED.description,
    input_format = EXCLUDED.input_format,
    output_format = EXCLUDED.output_format,
    sample_input = EXCLUDED.sample_input,
    sample_output = EXCLUDED.sample_output,
    visibility = EXCLUDED.visibility;

WITH tag_vocab AS (
    SELECT ARRAY[
        '双指针',
        '滑动窗口',
        '动态规划',
        '贪心',
        '图论',
        '搜索',
        '数论',
        '数据结构',
        '状态压缩',
        '排序',
        '哈希表',
        '连通性',
        '前缀和',
        '二分'
    ]::text[] AS tags
),
generated_problem_rows AS (
    SELECT
        code,
        substring(code FROM 2)::integer AS n
    FROM problems
    WHERE code ~ '^T[0-9]{10}$'
),
generated_problem_tag_seed AS (
    SELECT
        generated_problem_rows.code AS problem_code,
        selected.tag_name
    FROM generated_problem_rows
    CROSS JOIN tag_vocab
    CROSS JOIN LATERAL (
        VALUES
            (tag_vocab.tags[((generated_problem_rows.n - 1) % array_length(tag_vocab.tags, 1)) + 1]),
            (tag_vocab.tags[((generated_problem_rows.n + 3) % array_length(tag_vocab.tags, 1)) + 1]),
            (tag_vocab.tags[((generated_problem_rows.n + 7) % array_length(tag_vocab.tags, 1)) + 1])
    ) AS selected(tag_name)
    WHERE generated_problem_rows.n BETWEEN 33 AND 1000000
)
INSERT INTO problem_tags(problem_id, tag_id)
SELECT p.id, t.id
FROM generated_problem_tag_seed
JOIN problems p ON p.code = generated_problem_tag_seed.problem_code
JOIN tags t ON t.name = generated_problem_tag_seed.tag_name
ON CONFLICT DO NOTHING;
