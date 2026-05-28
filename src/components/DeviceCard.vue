<script setup lang="ts">
import { NTag, NSpin } from 'naive-ui'
import type { BleDevice } from '../composables/useBLE'

defineProps<{
  device: BleDevice
}>()

function getStatusTagType(status: BleDevice['configStatus']) {
  switch (status) {
    case 'success': return 'success'
    case 'failed': return 'error'
    case 'pending': return 'warning'
    default: return 'default'
  }
}

function getStatusText(status: BleDevice['configStatus'], isConnected: boolean, isConfiguring: boolean) {
  if (isConfiguring) return '配置中...'
  switch (status) {
    case 'success': return '已配置'
    case 'failed': return '失败'
    case 'pending': return '待配置'
    default: return isConnected ? '已连接' : '未连接'
  }
}
</script>

<template>
  <div class="device-card" :class="{ 'is-configuring': device.isConfiguring }">
    <div class="device-info">
      <div class="device-name">{{ device.name }}</div>
      <div class="device-id">{{ device.id }}</div>
    </div>
    <div class="device-status">
      <NTag v-if="device.isConfiguring" size="small" type="warning">
        <template #icon>
          <NSpin size="small" />
        </template>
        配置中
      </NTag>
      <NTag v-else size="small" :type="getStatusTagType(device.configStatus)">
        {{ getStatusText(device.configStatus, device.isConnected, device.isConfiguring) }}
      </NTag>
    </div>
  </div>
</template>

<style scoped>
.device-card {
  flex: 1;
  padding: 12px;
  border: 1px solid var(--border);
  border-radius: 8px;
  background: var(--bg);
  transition: border-color 0.2s, box-shadow 0.2s;
}

.device-card:hover {
  border-color: var(--accent);
}

.device-card.is-configuring {
  border-color: var(--accent);
  box-shadow: 0 0 8px var(--accent-bg);
}

.device-info {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.device-name {
  font-weight: 500;
  color: var(--text-h);
}

.device-id {
  font-size: 12px;
  color: var(--text);
  font-family: var(--mono);
}

.device-status {
  margin-top: 8px;
}
</style>