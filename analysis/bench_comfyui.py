"""Image-generation speed and power of a running ComfyUI, through its HTTP API.

  python -I analysis/bench_comfyui.py run --pci 0000:05:00.0 --label before-fp16acc \
      --out results/rtx3060/comfyui [--workflow api.json] [--runs 5]
  python -I analysis/bench_comfyui.py compare results/rtx3060/comfyui --base before-fp16acc --test after-fp16acc

The workflow is the most recent one in ComfyUI's history unless --workflow
names an API-format JSON. It is sanitised before use: every CLIPTextEncode
text is replaced by a fixed neutral prompt, LoRA loaders are bypassed, and
SaveImage becomes PreviewImage so nothing lands in the output folder. The
sanitised graph is stored next to the results, so a run can be repeated.

Each measured run changes only the sampler seed, so ComfyUI keeps the models
and the text encoding cached and the time is sampling plus VAE decode. Runs
alternate between the workflow's step count S and 2S: (t(2S) - t(S)) / S is the
time per sampling step, t(S) - S x that is the fixed cost (VAE decode, preview).
Power comes from NVML on the card at --pci, together with the PCIe traffic,
which on an x1 link shows whether weights are streamed from host memory.
"""
import argparse
import copy
import datetime
import hashlib
import json
from pathlib import Path
import statistics
import sys
import time
import urllib.request
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parent))
import nvml_sampler  # noqa: E402

POSITIVE = 'a lighthouse on a rocky coast at sunset, detailed illustration, soft light'
NEGATIVE = 'lowres, blurry'


def api(server, path, body=None):
    data = None if body is None else json.dumps(body).encode()
    request = urllib.request.Request(server + path, data=data,
                                     headers={'Content-Type': 'application/json'} if data else {})
    with urllib.request.urlopen(request, timeout=60) as response:
        return json.loads(response.read().decode('utf-8'))


def sanitise(graph):
    """Neutral prompts, no LoRA, preview instead of save. Returns (graph, sampler id)."""
    graph = copy.deepcopy(graph)
    negative = set()
    for node in graph.values():
        if 'negative' in node.get('inputs', {}) and isinstance(node['inputs']['negative'], list):
            negative.add(node['inputs']['negative'][0])
    for nid, node in graph.items():
        if node['class_type'] == 'CLIPTextEncode':
            node['inputs']['text'] = NEGATIVE if nid in negative else POSITIVE
        elif 'TextEncode' in node['class_type']:
            sys.exit('unsupported text encoder node %s (%s); pass --workflow' % (nid, node['class_type']))
    # Bypass LoRA loaders: point every consumer at the loader's own inputs.
    for nid in [n for n, node in graph.items() if node['class_type'].startswith('Lora')]:
        inputs = graph[nid]['inputs']
        sources = {0: inputs.get('model'), 1: inputs.get('clip')}
        for node in graph.values():
            for key, value in node['inputs'].items():
                if isinstance(value, list) and value[0] == nid:
                    if sources.get(value[1]) is None:
                        sys.exit('cannot bypass LoRA node %s' % nid)
                    node['inputs'][key] = sources[value[1]]
        del graph[nid]
    for node in graph.values():
        if node['class_type'] == 'SaveImage':
            node['class_type'] = 'PreviewImage'
            node['inputs'] = {'images': node['inputs']['images']}
    samplers = [nid for nid, node in graph.items()
                if 'steps' in node['inputs'] and ('seed' in node['inputs'] or 'noise_seed' in node['inputs'])]
    if len(samplers) != 1:
        sys.exit('need exactly one sampler with steps and seed, found %d; pass --workflow' % len(samplers))
    return graph, samplers[0]


def latest_workflow(server):
    history = api(server, '/history?max_items=1')
    if not history:
        sys.exit('ComfyUI history is empty; run the workflow once or pass --workflow')
    entry = next(iter(history.values()))
    return entry['prompt'][2]


def execute(server, graph, sampler, gpu):
    prompt_id = api(server, '/prompt', {'prompt': graph, 'client_id': str(uuid.uuid4())})['prompt_id']
    e0, t0 = gpu.energy_mj(), time.time()
    while True:
        history = api(server, '/history/' + prompt_id)
        if prompt_id in history and history[prompt_id].get('status', {}).get('completed') is not None:
            break
        time.sleep(0.05)
    t1, e1 = time.time(), gpu.energy_mj()
    entry = history[prompt_id]
    status = entry['status']
    if status.get('status_str') != 'success':
        sys.exit('prompt failed: %s' % json.dumps(status)[:2000])
    stamps = {m[0]: m[1] for m in status['messages']}
    start = stamps['execution_start']['timestamp'] / 1000.0
    end = stamps['execution_success']['timestamp'] / 1000.0
    cached = stamps.get('execution_cached', {}).get('nodes', [])
    return {'prompt_id': prompt_id, 'exec_seconds': round(end - start, 4), 'wall_seconds': round(t1 - t0, 4),
            'exec_start': start, 'exec_end': end,
            # Driver counter from submit to completion: includes queueing and polling.
            'counter_energy_j_wall': None if e0 is None or e1 is None else round((e1 - e0) / 1000.0, 3),
            'cached_nodes': cached}


def run(args):
    server = args.server.rstrip('/')
    stats = api(server, '/system_stats')
    queue = api(server, '/queue')
    if queue['queue_running'] or queue['queue_pending']:
        sys.exit('ComfyUI queue is not empty; wait until it is idle')
    gpu = nvml_sampler.Gpu(args.pci)
    devices = [d['name'] for d in stats['devices'] if d.get('type') == 'cuda']
    if len(devices) != 1 or gpu.name not in devices[0]:
        sys.exit('ComfyUI runs on %r, not on %s at %s' % (devices, gpu.name, args.pci))
    # After the first session the latest history entry is one of our own 2S
    # runs, so later sessions reuse the stored workflow instead.
    stored = sorted(args.out.glob('workflow-*.json')) if args.out.is_dir() else []
    if args.workflow:
        source = json.loads(Path(args.workflow).read_text(encoding='utf-8'))
    elif len(stored) == 1:
        source = json.loads(stored[0].read_text(encoding='utf-8'))
        print('reusing', stored[0])
    elif stored:
        sys.exit('several workflow-*.json in %s; pick one with --workflow' % args.out)
    else:
        source = latest_workflow(server)
    graph, sampler = sanitise(source)
    base_steps = int(graph[sampler]['inputs']['steps'])
    seed_key = 'seed' if 'seed' in graph[sampler]['inputs'] else 'noise_seed'
    graph[sampler]['inputs'][seed_key] = 0  # every measured prompt sets its own
    text = json.dumps(graph, sort_keys=True, indent=1)
    workflow_sha = hashlib.sha256(text.encode()).hexdigest()
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / ('workflow-%s.json' % workflow_sha[:12])).write_text(text + '\n', encoding='utf-8', newline='\n')

    def with_seed(seed, steps):
        g = copy.deepcopy(graph)
        g[sampler]['inputs'][seed_key] = seed
        g[sampler]['inputs']['steps'] = steps
        return g

    runs, warmup = [], []
    started = datetime.datetime.now(datetime.timezone.utc)
    with nvml_sampler.Sampler(gpu) as sampler_thread:
        seed = args.seed
        # Warmup: model load (slow over x1) and kernel selection, at both step counts.
        for steps in (base_steps, base_steps * 2):
            for _ in range(args.warmup):
                r = execute(server, with_seed(seed, steps), sampler, gpu)
                seed += 1
                warmup.append({'steps': steps, 'exec_seconds': r['exec_seconds'], 'wall_seconds': r['wall_seconds']})
        for i in range(args.runs):
            for steps in (base_steps, base_steps * 2):
                time.sleep(args.rest)
                r = execute(server, with_seed(seed, steps), sampler, gpu)
                r.update(steps=steps, seed=seed, **sampler_thread.window(r['exec_start'], r['exec_end']))
                seed += 1
                runs.append(r)
                print('%s: %d steps %.3f s, %s W, rx %s MB/s' % (args.label, steps, r['exec_seconds'],
                                                                 r['avg_power_w'], r['median_pcie_rx_mbs']))
    gpu.close()

    def med(steps, key):
        v = [r[key] for r in runs if r['steps'] == steps and r[key] is not None]
        return statistics.median(v) if v else None
    t1, t2 = med(base_steps, 'exec_seconds'), med(base_steps * 2, 'exec_seconds')
    per_step = (t2 - t1) / base_steps
    summary = {
        'steps': base_steps,
        'median_seconds_per_image': round(t1, 4),
        'images_per_minute': round(60 / t1, 3),
        'seconds_per_step': round(per_step, 4),
        'fixed_seconds_per_image': round(t1 - base_steps * per_step, 4),
        # Integrated over ComfyUI's own execution window only.
        'median_energy_j_per_image': med(base_steps, 'sampled_energy_j'),
        'median_avg_power_w': med(base_steps, 'avg_power_w'),
        'median_sm_mhz': med(base_steps, 'median_sm_mhz'),
        'median_pcie_rx_mbs': med(base_steps, 'median_pcie_rx_mbs'),
        'max_temp_c': max(r['max_temp_c'] for r in runs if r['max_temp_c'] is not None),
    }
    record = {
        'schema': 1, 'kind': 'comfyui', 'label': args.label, 'note': args.note,
        'started_utc': started.isoformat(timespec='seconds'),
        'gpu': gpu.name, 'pci': args.pci,
        'comfyui': {k: stats['system'].get(k) for k in ('comfyui_version', 'pytorch_version', 'python_version', 'argv')},
        'fp16_accumulation': '--fast' in stats['system'].get('argv', []) and
                             'fp16_accumulation' in stats['system'].get('argv', []),
        'workflow_sha256': workflow_sha, 'sampler_node': sampler,
        'summary': summary, 'warmup': warmup, 'runs': runs,
    }
    path = args.out / ('comfyui-%s-%s.json' % (args.label, started.strftime('%Y%m%dT%H%M%SZ')))
    if path.exists():
        sys.exit('refusing to overwrite %s' % path)
    path.write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps(summary, indent=2))
    print('saved', path)


def compare(args):
    def load(label):
        sessions = [json.loads(p.read_text(encoding='utf-8')) for p in sorted(args.directory.glob('comfyui-%s-*.json' % label))]
        sessions = [s for s in sessions if s.get('label') == label]
        if not sessions:
            sys.exit('no comfyui-%s-*.json in %s' % (label, args.directory))
        return sessions
    base, test = load(args.base), load(args.test)
    keys = ('gpu', 'pci', 'workflow_sha256')
    for s in base + test:
        for k in keys:
            if s[k] != base[0][k]:
                sys.exit('not comparable: %s differs (%r vs %r)' % (k, s[k], base[0][k]))
    for side, sessions in ((args.base, base), (args.test, test)):
        versions = {json.dumps(s['comfyui'], sort_keys=True) for s in sessions}
        modes = {s['fp16_accumulation'] for s in sessions}
        print('- %s: %d session(s), fp16_accumulation=%s, %s' % (side, len(sessions), sorted(modes),
                                                                  '; '.join(sorted(versions))))
    print()
    rows = (('median_seconds_per_image', 's/image', '%.3f'), ('images_per_minute', 'images/min', '%.2f'),
            ('seconds_per_step', 's/step', '%.4f'), ('fixed_seconds_per_image', 'fixed s/image', '%.3f'),
            ('median_energy_j_per_image', 'J/image', '%.1f'), ('median_avg_power_w', 'avg W', '%.1f'),
            ('median_sm_mhz', 'SM MHz', '%.0f'), ('median_pcie_rx_mbs', 'PCIe rx MB/s', '%.1f'),
            ('max_temp_c', 'max C', '%.0f'))
    print('| metric | %s | %s | %s/%s |' % (args.base, args.test, args.test, args.base))
    print('|---|---|---|---|')
    for key, title, fmt in rows:
        cells, meds = [], []
        for sessions in (base, test):
            values = [s['summary'][key] for s in sessions if s['summary'][key] is not None]
            med = statistics.median(values) if values else None
            meds.append(med)
            if med is None:
                cells.append('-')
            elif len(values) == 1:
                cells.append(fmt % med)
            else:
                cells.append('%s (%s-%s)' % (fmt % med, fmt % min(values), fmt % max(values)))
        ratio = '%.2f' % (meds[1] / meds[0]) if meds[0] and meds[1] is not None else '-'
        print('| %s | %s | %s | %s |' % (title, cells[0], cells[1], ratio))
    print('\nMedian over sessions (each session is itself a median over its runs). '
          'Board power from NVML, not wall power.')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    r = sub.add_parser('run')
    r.add_argument('--pci', required=True)
    r.add_argument('--label', required=True)
    r.add_argument('--out', type=Path, required=True)
    r.add_argument('--server', default='http://127.0.0.1:8188')
    r.add_argument('--workflow', help='API-format workflow JSON (default: latest history entry)')
    r.add_argument('--runs', type=int, default=5)
    r.add_argument('--warmup', type=int, default=2, help='warmup prompts per step count')
    r.add_argument('--rest', type=float, default=2.0, help='seconds idle before each measured prompt')
    r.add_argument('--seed', type=int, default=1000)
    r.add_argument('--note', default='')
    c = sub.add_parser('compare')
    c.add_argument('directory', type=Path)
    c.add_argument('--base', required=True)
    c.add_argument('--test', required=True)
    args = parser.parse_args()
    if args.command == 'run':
        if args.runs < 2 or not args.label.replace('-', '').replace('_', '').isalnum():
            parser.error('--runs must be >= 2 and --label alphanumeric (with - or _)')
        run(args)
    else:
        compare(args)


if __name__ == '__main__':
    main()
