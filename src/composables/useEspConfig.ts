export interface EspConfig {
  wifiSsid: string
  wifiPassword: string
  deviceName: string
  devicePrefix: string
}

export interface EspConfigResult {
  success: boolean
  message: string
}

const WIFI_SERVICE_UUID = '0000ffe0-0000-1000-8000-00805f9b34fb'
const WIFI_CHAR_UUID = '0000ffe1-0000-1000-8000-00805f9b34fb'

export function useEspConfig() {

  function encodeConfig(config: EspConfig): string {
    return JSON.stringify({
      type: 'wifi_config',
      ssid: config.wifiSsid,
      pass: config.wifiPassword,
      name: config.deviceName,
      prefix: config.devicePrefix
    })
  }

  function decodeResponse(data: string): EspConfigResult {
    try {
      const json = JSON.parse(data)
      if (json.result === 'ok') {
        return { success: true, message: '配置成功' }
      }
      return { success: false, message: json.error || '未知错误' }
    } catch {
      return { success: false, message: '响应解析失败' }
    }
  }

  async function configDevice(
    deviceId: string,
    getServerFn: (id: string) => any,
    config: EspConfig
  ): Promise<EspConfigResult> {
    try {
      const server = getServerFn(deviceId)
      if (!server) {
        return { success: false, message: '设备未连接' }
      }
      const service = await server.getPrimaryService(WIFI_SERVICE_UUID)
      const characteristic = await service.getCharacteristic(WIFI_CHAR_UUID)

      const data = encodeConfig(config)
      const encoder = new TextEncoder()
      await characteristic.writeValue(encoder.encode(data))

      return { success: true, message: '配置已发送' }
    } catch (e: any) {
      return { success: false, message: e.message }
    }
  }

  function validateConfig(config: EspConfig): { valid: boolean; errors: string[] } {
    const errors: string[] = []

    if (!config.wifiSsid || config.wifiSsid.length === 0) {
      errors.push('WiFi SSID 不能为空')
    }
    if (config.wifiSsid.length > 32) {
      errors.push('WiFi SSID 不能超过 32 字符')
    }
    if (config.wifiPassword.length > 64) {
      errors.push('WiFi 密码不能超过 64 字符')
    }
    if (config.devicePrefix && config.devicePrefix.length > 16) {
      errors.push('设备前缀不能超过 16 字符')
    }

    return { valid: errors.length === 0, errors }
  }

  return {
    encodeConfig,
    decodeResponse,
    configDevice,
    validateConfig
  }
}