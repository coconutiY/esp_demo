import { ref, computed } from 'vue'
import type { BleDevice } from './useBLE'
import type { EspConfig } from './useEspConfig'

export interface BatchProgress {
  total: number
  success: number
  failed: number
  current: string | null
  isRunning: boolean
}

export interface LogEntry {
  timestamp: Date
  level: 'info' | 'success' | 'error' | 'warn'
  message: string
  deviceId?: string
}

const progress = ref<BatchProgress>({
  total: 0,
  success: 0,
  failed: 0,
  current: null,
  isRunning: false
})

const logs = ref<LogEntry[]>([])

export function useBatchConfig() {

  function addLog(level: LogEntry['level'], message: string, deviceId?: string) {
    logs.value.push({
      timestamp: new Date(),
      level,
      message,
      deviceId
    })
  }

  function clearLogs() {
    logs.value = []
  }

  async function batchConfig(
    devices: BleDevice[],
    config: EspConfig,
    connectFn: (id: string) => Promise<boolean>,
    configFn: (deviceId: string, config: EspConfig) => Promise<{ success: boolean; message: string }>
  ) {
    const targets = devices
    progress.value = {
      total: targets.length,
      success: 0,
      failed: 0,
      current: null,
      isRunning: true
    }

    addLog('info', `开始批量配置，共 ${targets.length} 台设备`)

    for (const device of targets) {
      progress.value.current = device.name
      device.isConfiguring = true
      device.configStatus = 'pending'

      addLog('info', `正在配置 ${device.name}...`, device.id)

      try {
        const connected = await connectFn(device.id)
        if (!connected) {
          throw new Error('连接失败')
        }

        const result = await configFn(device.id, config)
        if (result.success) {
          progress.value.success++
          device.configStatus = 'success'
          addLog('success', `${device.name} 配置成功`, device.id)
        } else {
          throw new Error(result.message)
        }
      } catch (e: any) {
        progress.value.failed++
        device.configStatus = 'failed'
        addLog('error', `${device.name} 配置失败: ${e.message}`, device.id)
      } finally {
        device.isConfiguring = false
        progress.value.current = null
      }
    }

    progress.value.isRunning = false
    addLog('info', `批量配置完成: 成功 ${progress.value.success}，失败 ${progress.value.failed}`)
  }

  function stopBatch() {
    progress.value.isRunning = false
    addLog('warn', '用户中止批量配置')
  }

  const progressPercent = computed(() => {
    if (progress.value.total === 0) return 0
    const done = progress.value.success + progress.value.failed
    return Math.round((done / progress.value.total) * 100)
  })

  return {
    progress,
    logs,
    batchConfig,
    stopBatch,
    addLog,
    clearLogs,
    progressPercent
  }
}