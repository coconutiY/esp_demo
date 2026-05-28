<script setup lang="ts">
import { NButton, NScrollbar } from 'naive-ui'
import type { LogEntry } from '../composables/useBatchConfig'

defineProps<{
  logs: LogEntry[]
}>()

const emit = defineEmits<{
  (e: 'clear'): void
}>()

function formatTime(date: Date): string {
  return date.toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit', second: '2-digit' })
}

function getLogClass(level: LogEntry['level']): string {
  return `log-${level}`
}
</script>

<template>
  <div class="log-console">
    <div class="log-header">
      <span>日志</span>
      <NButton size="small" @click="emit('clear')">清空</NButton>
    </div>
    <NScrollbar class="log-content">
      <div
        v-for="(log, index) in logs"
        :key="index"
        class="log-entry"
        :class="getLogClass(log.level)"
      >
        <span class="log-time">{{ formatTime(log.timestamp) }}</span>
        <span class="log-level">[{{ log.level.toUpperCase() }}]</span>
        <span class="log-message">{{ log.message }}</span>
      </div>
      <div v-if="logs.length === 0" class="empty-log">
        暂无日志
      </div>
    </NScrollbar>
  </div>
</template>

<style scoped>
.log-console {
  padding: 16px;
  border-top: 1px solid var(--border);
  display: flex;
  flex-direction: column;
  height: 200px;
}

.log-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 8px;
}

.log-content {
  flex: 1;
  font-family: var(--mono);
  font-size: 13px;
  background: var(--code-bg);
  border-radius: 4px;
  padding: 8px;
}

.log-entry {
  display: flex;
  gap: 8px;
  padding: 4px 0;
  border-bottom: 1px solid var(--border);
}

.log-time {
  color: var(--text);
  flex-shrink: 0;
}

.log-level {
  flex-shrink: 0;
  width: 60px;
}

.log-info .log-level {
  color: #1890ff;
}

.log-success .log-level {
  color: #52c41a;
}

.log-error .log-level {
  color: #ff4d4f;
}

.log-warn .log-level {
  color: #faad14;
}

.log-message {
  flex: 1;
}

.empty-log {
  text-align: center;
  color: var(--text);
  padding: 16px;
}
</style>