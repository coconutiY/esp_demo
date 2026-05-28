<script setup lang="ts">
import { NButton, NTag } from 'naive-ui'
import { useBLE } from '../composables/useBLE'

const { isScanning, devices, startScan, stopScan } = useBLE()
</script>

<template>
  <div class="ble-scanner">
    <div class="scanner-header">
      <NSpace align="center">
        <NButton
          type="primary"
          :disabled="isScanning"
          @click="startScan"
        >
          {{ isScanning ? '扫描中...' : '开始扫描' }}
        </NButton>
        <NButton
          v-if="isScanning"
          @click="stopScan"
        >
          停止
        </NButton>
        <NTag type="info">
          已发现 {{ devices.length }} 台设备
        </NTag>
      </NSpace>
    </div>
  </div>
</template>

<style scoped>
.ble-scanner {
  padding: 16px;
  border-bottom: 1px solid var(--border);
}

.scanner-header {
  display: flex;
  align-items: center;
  gap: 16px;
}
</style>