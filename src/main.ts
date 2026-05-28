import { createApp } from 'vue'
import {
  create,
  NButton,
  NInput,
  NSelect,
  NForm,
  NFormItem,
  NCard,
  NSpace,
  NText,
  NAlert,
  NSpin,
  NList,
  NListItem,
  NTag,
  NBadge,
  NIcon,
  NModal
} from 'naive-ui'
import './style.css'
import App from './App.vue'

const naive = create({
  components: [
    NButton,
    NInput,
    NSelect,
    NForm,
    NFormItem,
    NCard,
    NSpace,
    NText,
    NAlert,
    NSpin,
    NList,
    NListItem,
    NTag,
    NBadge,
    NIcon,
    NModal
  ]
})

const app = createApp(App)
app.use(naive)
app.mount('#app')
