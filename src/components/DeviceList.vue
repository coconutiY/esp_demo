<script setup lang="ts">
import { NCheckbox } from 'naive-ui'
import type { BleDevice } from '../composables/useBLE'
import DeviceCard from './DeviceCard.vue'

const props = defineProps<{
  devices: BleDevice[]
  selectedIds: string[]
}>()

const emit = defineEmits<{
  (e: 'update:selectedIds', ids: string[]): void
}>()

function toggleDevice(id: string) {
  const current = [...props.selectedIds]
  const index = current.indexOf(id)
  if (index === -1) {
    current.push(id)
  } else {
    current.splice(index, 1)
  }
  emit('update:selectedIds', current)
}
</script>

<template>
  <div class="device-list">
    <div class="list-header">
      <span>设备列表</span>
      <span class="device-count">{{ devices.length }} 台</span>
    </div>
    <div class="devices">
      <div
        v-for="device in devices"
        :key="device.id"
        class="device-item"
        @click="toggleDevice(device.id)"
      >
        <NCheckbox
          :checked="selectedIds.includes(device.id)"
          @click.stop
          @update:checked="toggleDevice(device.id)"
        />
        <DeviceCard :device="device" />
      </div>
    </div>
    <div v-if="devices.length === 0" class="empty-state">
      暂无设备，请点击扫描
    </div>
  </div>
</template>

<style scoped>
.device-list {
  padding: 16px;
  border-right: 1px solid var(--border);
  min-width: 300px;
  max-width: 400px;
}

.list-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 16px;
  font-weight: 500;
}

.device-count {
  color: var(--text);
  font-size: 14px;
}

.devices {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.device-item {
  display: flex;
  align-items: flex-start;
  gap: 12px;
  cursor: pointer;
}

.empty-state {
  text-align: center;
  padding: 32px;
  color: var(--text);
}
</style>