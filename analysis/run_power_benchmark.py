"""Run power-benchmark.exe several times on one GPU and keep every run.

Each run is saved as its own JSON file, wrapped with the label it belongs to
(for example "before" or "after"), the executable's SHA-256, the arguments and
the time, so before/after sets can be compared later with compare_power.py.
Nothing here changes clocks, power limits or the driver.

  python -I analysis/run_power_benchmark.py --pci 0000:05:00.0 --label before \
      --runs 3 --out results/rtx3060
"""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent


def busy_processes(pci):
    """Compute processes on the target GPU, as nvidia-smi reports them."""
    try:
        out = subprocess.run(
            ['nvidia-smi', '--query-compute-apps=pid,process_name,gpu_bus_id', '--format=csv,noheader'],
            capture_output=True, text=True, timeout=30, check=True).stdout
    except (OSError, subprocess.SubprocessError):
        return None
    want = pci.lower().split(':', 1)[-1]  # nvidia-smi prints an 8-digit domain
    return [line.strip() for line in out.splitlines() if line.strip() and line.lower().rstrip().endswith(want)]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--pci', required=True, help='PCI bus ID of the GPU, e.g. 0000:05:00.0')
    parser.add_argument('--label', required=True, help='state being measured, e.g. before or after')
    parser.add_argument('--runs', type=int, default=3)
    parser.add_argument('--out', type=Path, required=True, help='directory for the per-run JSON files')
    parser.add_argument('--seconds', type=float, default=15)
    parser.add_argument('--warmup', type=float, default=5)
    parser.add_argument('--idle', type=float, default=5)
    parser.add_argument('--cooldown', type=float, default=30, help='seconds to rest between runs')
    parser.add_argument('--note', default='', help='free text stored with every run (boot, slot, ambient...)')
    parser.add_argument('--allow-busy', action='store_true',
                        help='measure even with other processes on the GPU, recording them in each run')
    parser.add_argument('--exe', type=Path, default=HERE / 'power-benchmark.exe')
    args = parser.parse_args()
    if args.runs < 1 or not args.label.replace('-', '').replace('_', '').isalnum():
        parser.error('--runs must be >= 1 and --label alphanumeric (with - or _)')
    if not args.exe.is_file():
        parser.error('%s not found; run analysis/build-power-benchmark.cmd first' % args.exe)

    busy = busy_processes(args.pci)
    if busy and not args.allow_busy:
        sys.exit('other compute processes are using %s:\n  %s\n(a GPU that drives the display always has '
                 'some; --allow-busy measures anyway and records them)' % (args.pci, '\n  '.join(busy)))
    exe_sha = hashlib.sha256(args.exe.read_bytes()).hexdigest()
    command = [str(args.exe), '--pci', args.pci, '--seconds', str(args.seconds),
               '--warmup', str(args.warmup), '--idle', str(args.idle)]
    args.out.mkdir(parents=True, exist_ok=True)
    for run in range(1, args.runs + 1):
        if run > 1:
            time.sleep(args.cooldown)
        started = datetime.datetime.now(datetime.timezone.utc)
        sharing = busy_processes(args.pci) if args.allow_busy else []
        proc = subprocess.run(command, capture_output=True, text=True)
        if proc.returncode != 0:
            sys.exit('run %d failed (exit %d):\n%s' % (run, proc.returncode, proc.stderr.strip()))
        result = json.loads(proc.stdout)
        record = {
            'schema': 1,
            'label': args.label,
            'run': run,
            'started_utc': started.isoformat(timespec='seconds'),
            'note': args.note,
            'executable_sha256': exe_sha,
            'arguments': command[1:],
            # Other processes holding a context on the card when this run began
            # (only with --allow-busy): display, browser and desktop apps on a GPU
            # that drives a monitor.
            'shared_with': sharing,
            'result': result,
        }
        name = '%s-%s-run%02d.json' % (args.label, started.strftime('%Y%m%dT%H%M%SZ'), run)
        path = args.out / name
        if path.exists():
            sys.exit('refusing to overwrite %s' % path)
        path.write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8', newline='\n')
        fp32acc = next(b for b in result['benchmarks'] if b['name'] == 'fp16_tensor_fp32_accumulate')
        print('%s run %d: fp16/fp32-acc %.2f TFLOPS at %.1f W (%s)' % (
            args.label, run, fp32acc['tflops'], fp32acc['avg_power_w'], path))


if __name__ == '__main__':
    main()
