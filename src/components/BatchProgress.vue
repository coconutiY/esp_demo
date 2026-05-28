<script setup lang="ts">
import { NProgress, NText, NSpace } from 'naive-ui'
import type { BatchProgress } from '../composables/useBatchConfig'

defineProps<{
  progress: BatchProgress
  percent: number
}>()
</script>

<template>
  <div class="batch-progress">
    <div class="progress-header">
      <span>批量进度</span>
      <NSpace>
        <NText type="success">成功: {{ progress.success }}</NText>
        <NText type="error">失败: {{ progress.failed }}</NText>
        <NText type="info" v-if="progress.current">当前: {{ progress.current }}</NText>
      </NSpace>
    </div>
    <NProgress
      :percentage="percent"
      :status="progress.failed > 0 ? 'error' : undefined"
      :processing="progress.isRunning"
    />
    <div class="progress-detail">
      {{ progress.success + progress.failed }} / {{ progress.total }}
    </div>
  </div>
</template>

<style scoped>
.batch-progress {
  padding: 16px;
  border-top: 1px solid var(--border);
}

.progress-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 12px;
}

.progress-detail {
  text-align: center;
  margin-top: 8px;
  font-size: 14px;
  color: var(--text);
}
</style>