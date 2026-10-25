"""Background NVML sampling of one GPU, chosen by PCI bus ID (ctypes, no packages).

Board power, SM/memory clock, temperature, PCIe link state and PCIe traffic,
plus the driver's cumulative energy counter. Read-only: nothing is set.
"""
import ctypes
import statistics
import sys
import threading
import time

NVML_CLOCK_SM, NVML_CLOCK_MEM = 1, 2
NVML_TEMPERATURE_GPU = 0
NVML_PCIE_UTIL_TX_BYTES, NVML_PCIE_UTIL_RX_BYTES = 0, 1


class NvmlError(RuntimeError):
    pass


class Gpu:
    def __init__(self, pci):
        name = 'nvml.dll' if sys.platform == 'win32' else 'libnvidia-ml.so.1'
        self.lib = ctypes.CDLL(name)
        self._call('nvmlInit_v2')
        self.handle = ctypes.c_void_p()
        self._call('nvmlDeviceGetHandleByPciBusId_v2', pci.encode(), ctypes.byref(self.handle))
        buf = ctypes.create_string_buffer(96)
        self._call('nvmlDeviceGetName', self.handle, buf, 96)
        self.name = buf.value.decode()
        self.pci = pci

    def _call(self, fn, *args):
        status = getattr(self.lib, fn)(*args)
        if status:
            self.lib.nvmlErrorString.restype = ctypes.c_char_p
            raise NvmlError('%s: %s' % (fn, self.lib.nvmlErrorString(status).decode()))

    def _uint(self, fn, *args):
        value = ctypes.c_uint()
        try:
            self._call(fn, self.handle, *args, ctypes.byref(value))
        except NvmlError:
            return None
        return value.value

    def energy_mj(self):
        value = ctypes.c_ulonglong()
        try:
            self._call('nvmlDeviceGetTotalEnergyConsumption', self.handle, ctypes.byref(value))
        except NvmlError:
            return None
        return value.value

    def sample(self):
        mw = self._uint('nvmlDeviceGetPowerUsage')
        return {
            't': time.time(),
            'watts': None if mw is None else mw / 1000.0,
            'sm_mhz': self._uint('nvmlDeviceGetClockInfo', NVML_CLOCK_SM),
            'mem_mhz': self._uint('nvmlDeviceGetClockInfo', NVML_CLOCK_MEM),
            'temp_c': self._uint('nvmlDeviceGetTemperature', NVML_TEMPERATURE_GPU),
            'pcie_gen': self._uint('nvmlDeviceGetCurrPcieLinkGeneration'),
            'pcie_width': self._uint('nvmlDeviceGetCurrPcieLinkWidth'),
            # KB/s over the driver's 20 ms window; this is what an x1 link carries.
            'pcie_rx_kbs': self._uint('nvmlDeviceGetPcieThroughput', NVML_PCIE_UTIL_RX_BYTES),
            'pcie_tx_kbs': self._uint('nvmlDeviceGetPcieThroughput', NVML_PCIE_UTIL_TX_BYTES),
        }

    def close(self):
        self.lib.nvmlShutdown()


class Sampler:
    """Samples continuously in a thread; window(t0, t1) summarises a time range."""

    def __init__(self, gpu, interval=0.1):
        self.gpu, self.interval = gpu, interval
        self.samples, self.lock, self.stop = [], threading.Lock(), threading.Event()
        self.thread = threading.Thread(target=self._run, daemon=True)

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, *exc):
        self.stop.set()
        self.thread.join()

    def _run(self):
        while not self.stop.is_set():
            s = self.gpu.sample()
            with self.lock:
                self.samples.append(s)
            self.stop.wait(self.interval)

    def window(self, t0, t1):
        with self.lock:
            ss = [s for s in self.samples if t0 <= s['t'] <= t1]
        def med(key):
            v = [s[key] for s in ss if s[key] is not None]
            return statistics.median(v) if v else None
        def top(key):
            v = [s[key] for s in ss if s[key] is not None]
            return max(v) if v else None
        joules = 0.0
        for a, b in zip(ss, ss[1:]):
            if a['watts'] is not None and b['watts'] is not None:
                joules += (b['t'] - a['t']) * (a['watts'] + b['watts']) / 2
        span = ss[-1]['t'] - ss[0]['t'] if len(ss) > 1 else 0
        return {
            'samples': len(ss),
            'sampled_energy_j': round(joules, 3),
            'avg_power_w': round(joules / span, 3) if span else None,
            'max_power_w': top('watts'),
            'median_sm_mhz': med('sm_mhz'),
            'median_mem_mhz': med('mem_mhz'),
            'max_temp_c': top('temp_c'),
            'pcie_gen': med('pcie_gen'),
            'pcie_width': med('pcie_width'),
            'median_pcie_rx_mbs': None if med('pcie_rx_kbs') is None else round(med('pcie_rx_kbs') / 1000, 1),
            'max_pcie_rx_mbs': None if top('pcie_rx_kbs') is None else round(top('pcie_rx_kbs') / 1000, 1),
            'median_pcie_tx_mbs': None if med('pcie_tx_kbs') is None else round(med('pcie_tx_kbs') / 1000, 1),
        }
