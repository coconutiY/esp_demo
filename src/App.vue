<script setup lang="ts">
import { ref, computed, reactive } from 'vue'
import { NLayout, NLayoutSider, NLayoutContent, NCard, NText } from 'naive-ui'
import BleScanner from './components/BleScanner.vue'
import DeviceList from './components/DeviceList.vue'
import ConfigPanel from './components/ConfigPanel.vue'
import BatchProgress from './components/BatchProgress.vue'
import LogConsole from './components/LogConsole.vue'
import { useBLE, type BleDevice } from './composables/useBLE'
import { useEspConfig, type EspConfig } from './composables/useEspConfig'
import { useBatchConfig } from './composables/useBatchConfig'

const { devices, connectDevice, getServer } = useBLE()
const { configDevice, validateConfig } = useEspConfig()
const { progress, logs, batchConfig, clearLogs, progressPercent } = useBatchConfig()

const selectedIds = ref<string[]>([])
const config = reactive<EspConfig>({
  wifiSsid: '',
  wifiPassword: '',
  deviceName: '',
  devicePrefix: ''
})

const selectedDevices = computed(() =>
  devices.value.filter((d: BleDevice) => selectedIds.value.includes(d.id))
)

async function handleStartBatch() {
  const validation = validateConfig(config)
  if (!validation.valid) {
    alert(validation.errors.join('\n'))
    return
  }

  await batchConfig(
    selectedDevices.value,
    config,
    connectDevice,
    async (deviceId, cfg) => {
      return await configDevice(deviceId, getServer, cfg)
    }
  )
}
</script>

<template>
  <NLayout has-sider style="height: 100vh">
    <NLayoutSider :width="400" bordered>
      <div class="sider-layout">
        <BleScanner />
        <DeviceList
          :devices="devices"
          v-model:selected-ids="selectedIds"
        />
      </div>
    </NLayoutSider>

    <NLayoutContent>
      <div class="main-layout">
        <div class="header">
          <h1>ESP32 BLE 批量配置工具</h1>
          <NText type="secondary">通过 Web Bluetooth 批量配置 ESP32 设备</NText>
        </div>

        <NCard class="config-card">
          <ConfigPanel
            :config="config"
            :selected-count="selectedIds.length"
            :is-running="progress.isRunning"
            @update:config="Object.assign(config, $event)"
            @start-batch="handleStartBatch"
          />
        </NCard>

        <BatchProgress :progress="progress" :percent="progressPercent" />
        <LogConsole :logs="logs" @clear="clearLogs" />
      </div>
    </NLayoutContent>
  </NLayout>
</template>

<style scoped>
.sider-layout {
  display: flex;
  flex-direction: column;
  height: 100%;
}

.main-layout {
  display: flex;
  flex-direction: column;
  height: 100%;
  padding: 24px;
  gap: 16px;
}

.header {
  text-align: center;
}

.header h1 {
  margin: 0 0 8px 0;
}

.config-card {
  flex: 1;
  min-height: 0;
}
</style>