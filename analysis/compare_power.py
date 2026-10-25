"""Compare power-benchmark runs between two labels (default: before, after).

Reads the per-run JSON files written by run_power_benchmark.py, checks that
both sets come from the same card, driver, VBIOS, power limit and executable,
and prints a Markdown table: per precision the median over runs, the min-max
spread, and after/before ratios.

  python -I analysis/compare_power.py results/rtx3060 [--base before --test after]

How to read it: the FP16-accumulate control already runs at full rate on
GeForce, so its ratio should stay near 1.00; a real unlock shows up as the
FP16/BF16 FP32-accumulate rows moving towards the control. TFLOPS/GHz removes
the clock change a tighter power budget causes, GFLOPS/W is the efficiency.
"""
import argparse
import json
from pathlib import Path
import statistics
import sys

ROWS = (('tflops', 'TFLOPS', '%.2f'), ('avg_power_w', 'avg W', '%.1f'),
        ('gflops_per_w', 'GFLOPS/W', '%.1f'), ('median_sm_mhz', 'SM MHz', '%.0f'),
        ('tflops_per_ghz', 'TFLOPS/GHz', '%.3f'), ('max_temp_c', 'max C', '%.0f'))
# Fields that must agree for two runs to be comparable at all.
SAME = ('gpu', 'pci', 'vbios', 'driver', 'power_limit_w', 'cublas', 'seconds')


def load(directory, label):
    runs = []
    for path in sorted(Path(directory).glob(label + '-*.json')):
        record = json.loads(path.read_text(encoding='utf-8'))
        if record.get('schema') == 1 and record.get('label') == label:
            record['_path'] = path.name
            runs.append(record)
    return runs


def spread(values, fmt):
    med = statistics.median(values)
    if len(values) == 1:
        return med, fmt % med
    return med, '%s (%s-%s)' % (fmt % med, fmt % min(values), fmt % max(values))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--base', default='before')
    parser.add_argument('--test', default='after')
    args = parser.parse_args()
    sets = {label: load(args.directory, label) for label in (args.base, args.test)}
    for label, runs in sets.items():
        if not runs:
            sys.exit('no %s-*.json runs in %s' % (label, args.directory))

    problems = []
    first = sets[args.base][0]
    for label, runs in sets.items():
        for run in runs:
            for key in SAME:
                if run['result'].get(key) != first['result'].get(key):
                    problems.append('%s: %s=%r, %s has %r' % (run['_path'], key, run['result'].get(key),
                                                             first['_path'], first['result'].get(key)))
            if run['executable_sha256'] != first['executable_sha256']:
                problems.append('%s: different power-benchmark.exe' % run['_path'])
    if problems:
        print('**Not comparable:**\n')
        for problem in problems:
            print('- ' + problem)
        sys.exit(1)

    r = first['result']
    print('%s at %s, VBIOS %s, driver %s, power limit %.0f W, PCIe max gen%d x%d; %d %s run(s) vs %d %s run(s).\n' % (
        r['gpu'], r['pci'], r['vbios'], r['driver'], r['power_limit_w'], r['pcie_max_gen'], r['pcie_max_width'],
        len(sets[args.base]), args.base, len(sets[args.test]), args.test))
    names = [b['name'] for b in r['benchmarks']]
    print('| precision | metric | %s | %s | %s/%s |' % (args.base, args.test, args.test, args.base))
    print('|---|---|---|---|---|')
    for name in names:
        for key, title, fmt in ROWS:
            cells, meds = [], []
            for label in (args.base, args.test):
                values = [next(b for b in run['result']['benchmarks'] if b['name'] == name)[key] for run in sets[label]]
                med, text = spread(values, fmt)
                cells.append(text)
                meds.append(med)
            ratio = '%.2f' % (meds[1] / meds[0]) if meds[0] else '-'
            print('| %s | %s | %s | %s | %s |' % (name, title, cells[0], cells[1], ratio))
    print()
    print('| other | %s | %s | %s/%s |' % (args.base, args.test, args.test, args.base))
    print('|---|---|---|---|')
    others = (('idle avg W (context held)', lambda res: res['idle']['avg_power_w'], '%.1f'),
              ('host-to-device GB/s', lambda res: res['pcie_transfer']['host_to_device']['median_gb_s'], '%.2f'),
              ('device-to-host GB/s', lambda res: res['pcie_transfer']['device_to_host']['median_gb_s'], '%.2f'),
              ('PCIe link gen (under copy)', lambda res: res['pcie_transfer']['host_to_device']['pcie_gen'], '%.0f'),
              ('PCIe link width (under copy)', lambda res: res['pcie_transfer']['host_to_device']['pcie_width'], '%.0f'))
    for title, get, fmt in others:
        meds, cells = [], []
        for label in (args.base, args.test):
            med, text = spread([get(run['result']) for run in sets[label]], fmt)
            meds.append(med)
            cells.append(text)
        print('| %s | %s | %s | %s |' % (title, cells[0], cells[1], '%.2f' % (meds[1] / meds[0]) if meds[0] else '-'))
    print('\nMedian over runs; parentheses give the min-max range. Board power as reported by NVML, not wall power.')


if __name__ == '__main__':
    main()
