import { ref, computed } from 'vue'

export interface BleDevice {
  id: string
  name: string
  rssi: number
  isConnected: boolean
  isConfiguring: boolean
  configStatus: 'pending' | 'success' | 'failed' | null
}

declare global {
  interface Navigator {
    bluetooth: Bluetooth
  }

  interface Bluetooth {
    requestDevice(options: RequestDeviceOptions): Promise<BluetoothDevice>
    getDevice(options: { deviceId: string }): Promise<BluetoothDevice>
  }

  interface RequestDeviceOptions {
    filters?: BluetoothLEScanFilter[]
    optionalServices?: string[]
    acceptAllDevices?: boolean
  }

  interface BluetoothLEScanFilter {
    namePrefix?: string
    name?: string
    services?: string[]
  }

  interface BluetoothDevice extends EventTarget {
    id: string
    name: string
    gatt: BluetoothRemoteGATTServer | null
    addEventListener(type: 'gattserverdisconnected', listener: (event: Event) => void): void
    removeEventListener(type: 'gattserverdisconnected', listener: (event: Event) => void): void
  }

  interface BluetoothRemoteGATTServer {
    connect(): Promise<BluetoothRemoteGATTServer>
    disconnect(): void
    getPrimaryService(service: string): Promise<BluetoothRemoteGATTService>
  }

  interface BluetoothRemoteGATTService {
    getCharacteristic(characteristic: string): Promise<BluetoothRemoteGATTCharacteristic>
  }

  interface BluetoothRemoteGATTCharacteristic extends EventTarget {
    writeValue(value: BufferSource): Promise<void>
    readValue(): Promise<DataView>
    startNotifications(): Promise<void>
    stopNotifications(): Promise<void>
    addEventListener(type: 'characteristicvaluechanged', listener: (event: Event) => void): void
    removeEventListener(type: 'characteristicvaluechanged', listener: (event: Event) => void): void
    value: DataView | null
    uuid: string
  }
}

const devices = ref<BleDevice[]>([])
const isScanning = ref(false)
const error = ref<string | null>(null)
const deviceServers = new Map<string, BluetoothRemoteGATTServer>()

export function useBLE() {
  const discoveredDevices = computed(() => devices.value)

  async function startScan() {
    error.value = null
    isScanning.value = true
    devices.value = []

    try {
      const device = await navigator.bluetooth.requestDevice({
        filters: [{ namePrefix: 'ESP32' }],
        optionalServices: ['0000ffe0-0000-1000-8000-00805f9b34fb']
      })

      device.addEventListener('gattserverdisconnected', () => {
        const dev = devices.value.find((d: BleDevice) => d.id === device.id)
        if (dev) {
          dev.isConnected = false
          deviceServers.delete(device.id)
        }
      })

      addDevice(device)
    } catch (e: unknown) {
      const err = e as Error
      if (err.name !== 'NotFoundError' && err.name !== 'AbortError') {
        error.value = err.message
      }
    } finally {
      isScanning.value = false
    }
  }

  function stopScan() {
    isScanning.value = false
    deviceServers.forEach(server => server.disconnect())
    deviceServers.clear()
  }

  function addDevice(device: BluetoothDevice) {
    const existing = devices.value.find((d: BleDevice) => d.id === device.id)
    if (!existing) {
      devices.value.push({
        id: device.id,
        name: device.name || 'Unknown ESP32',
        rssi: 0,
        isConnected: false,
        isConfiguring: false,
        configStatus: null
      })
    }
  }

  async function connectDevice(deviceId: string): Promise<boolean> {
    try {
      const dev = devices.value.find((d: BleDevice) => d.id === deviceId)
      if (!dev) return false

      const device = await navigator.bluetooth.getDevice({ deviceId })
      const gatt = await device.gatt!.connect()
      deviceServers.set(deviceId, gatt)
      dev.isConnected = true
      return true
    } catch (e: unknown) {
      const err = e as Error
      error.value = err.message
      return false
    }
  }

  async function disconnectDevice(deviceId: string) {
    const server = deviceServers.get(deviceId)
    if (server) {
      server.disconnect()
      deviceServers.delete(deviceId)
    }
    const dev = devices.value.find((d: BleDevice) => d.id === deviceId)
    if (dev) dev.isConnected = false
  }

  async function sendConfig(deviceId: string, data: string): Promise<boolean> {
    const server = deviceServers.get(deviceId)
    if (!server) return false
    try {
      const service = await server.getPrimaryService('0000ffe0-0000-1000-8000-00805f9b34fb')
      const characteristic = await service.getCharacteristic('0000ffe1-0000-1000-8000-00805f9b34fb')
      const encoder = new TextEncoder()
      await characteristic.writeValue(encoder.encode(data))
      return true
    } catch (e: unknown) {
      const err = e as Error
      error.value = err.message
      return false
    }
  }

  function getServer(deviceId: string): BluetoothRemoteGATTServer | undefined {
    return deviceServers.get(deviceId)
  }

  return {
    devices: discoveredDevices,
    isScanning,
    error,
    startScan,
    stopScan,
    connectDevice,
    disconnectDevice,
    sendConfig,
    getServer
  }
}