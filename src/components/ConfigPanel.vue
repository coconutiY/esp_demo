<script setup lang="ts">
import { NForm, NFormItem, NInput, NButton, NAlert } from 'naive-ui'
import type { EspConfig } from '../composables/useEspConfig'

const props = defineProps<{
  config: EspConfig
  selectedCount: number
  isRunning: boolean
}>()

const emit = defineEmits<{
  (e: 'update:config', config: EspConfig): void
  (e: 'startBatch'): void
}>()

function updateConfig(key: keyof EspConfig, value: string) {
  emit('update:config', { ...props.config, [key]: value })
}
</script>

<template>
  <div class="config-panel">
    <h3>配置面板</h3>

    <NForm label-placement="left" label-width="100">
      <NFormItem label="WiFi SSID">
        <NInput
          :value="config.wifiSsid"
          placeholder="请输入 WiFi 名称"
          @update:value="(v) => updateConfig('wifiSsid', v)"
        />
      </NFormItem>

      <NFormItem label="WiFi 密码">
        <NInput
          :value="config.wifiPassword"
          type="password"
          placeholder="请输入 WiFi 密码"
          @update:value="(v) => updateConfig('wifiPassword', v)"
        />
      </NFormItem>

      <NFormItem label="设备前缀">
        <NInput
          :value="config.devicePrefix"
          placeholder="如: ESP_"
          @update:value="(v) => updateConfig('devicePrefix', v)"
        />
      </NFormItem>

      <NFormItem label="设备名称">
        <NInput
          :value="config.deviceName"
          placeholder="单个设备名称（批量时自动添加序号）"
          @update:value="(v) => updateConfig('deviceName', v)"
        />
      </NFormItem>
    </NForm>

    <div v-if="selectedCount === 0" class="select-hint">
      <NAlert type="info">
        请先在左侧选择要配置的设备
      </NAlert>
    </div>

    <div class="actions">
      <NButton
        type="primary"
        size="large"
        :disabled="selectedCount === 0 || isRunning || !config.wifiSsid"
        :loading="isRunning"
        @click="emit('startBatch')"
      >
        {{ isRunning ? '配置中...' : `批量配置 (${selectedCount} 台)` }}
      </NButton>
    </div>
  </div>
</template>

<style scoped>
.config-panel {
  padding: 16px;
  flex: 1;
}

.config-panel h3 {
  margin: 0 0 16px 0;
  font-size: 16px;
}

.select-hint {
  margin: 16px 0;
}

.actions {
  margin-top: 24px;
  text-align: center;
}
</style>