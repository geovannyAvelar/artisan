# Task Manager - Complex React App with Bundled Libraries

A production-like React application demonstrating:
- **Advanced React patterns**: hooks, state management, memoization, callbacks
- **Complex components**: forms, filtering, sorting, conditional rendering
- **Bundled npm libraries**: lodash and date-fns integration
- **Real-world features**: task management, priority levels, due dates, completion tracking

## Features

### Task Management
- ✅ Create tasks with title, due date, and priority
- ✅ Mark tasks complete/incomplete
- ✅ Delete tasks
- ✅ Automatic overdue detection
- ✅ Statistics tracking (total, active, completed, overdue)

### Filtering & Sorting
- **Filter by**: All, Active, Completed, Overdue
- **Sort by**: Due Date, Priority, Recently Created
- **Real-time updates** with useMemo optimization

### Advanced React Features
- **Hooks**: useState, useEffect, useCallback, useMemo, useRef
- **Form handling**: useForm pattern for input management
- **Performance**: useCallback and useMemo prevent unnecessary renders
- **Conditional rendering**: Smart display based on task state

## Project Structure

```
task-manager/
├── pages/
│   └── index.html           # HTML template with root div
├── app.jsx                  # Main React application
├── bundle-libs.js           # npm library bundling entry point
├── bundle.sh                # Bundling script
├── package.json             # npm dependencies (lodash, date-fns)
└── README.md                # This file
```

## Getting Started

### 1. Install Dependencies

```bash
cd examples/task-manager
npm install
```

This installs:
- `lodash` - Utility functions for array/object manipulation
- `date-fns` - Date formatting and manipulation
- `esbuild` - Fast bundler

### 2. Bundle Libraries (Optional - for production)

```bash
bash bundle.sh
```

This creates `bundles/libs.js` with all the npm code bundled together. In production, you'd then wrap it with the Python script to create a module.

### 3. Build & Run

```bash
artisan build . --run
```

### 4. Use the App

- **Add tasks** with title, due date, and priority
- **Filter tasks** by status (All/Active/Completed/Overdue)
- **Sort tasks** by due date, priority, or creation time
- **Track progress** with the completion rate
- **Mark complete** or **delete tasks**

## How It Works

### Without Bundling (Current Example)

The app uses **mocked versions** of lodash and date-fns functions:

```javascript
const LIB = {
  sortBy: (arr, key) => [...arr].sort(...),
  filter: (arr, fn) => arr.filter(fn),
  groupBy: (arr, key) => { /* ... */ }
};

const DATE = {
  format: (date, fmt) => /* ... */,
  isToday: (date) => /* ... */,
  differenceInDays: (d1, d2) => /* ... */
};
```

**Why?** This shows you can build complex apps immediately without waiting for bundling. The mocked functions have the same API as the real ones.

### With Bundling (Production)

To use the actual npm libraries:

1. Run `npm install && bash bundle.sh`
2. The bundler creates `bundles/libs.js` (minified, optimized)
3. Wrap with: `python3 ../wrap-bundle.py bundles/libs.js 'task-libs' > modules/libs-module.ts`
4. Register in your app:

```javascript
import { registerModule, require } from "art/modules";
// Load the wrapped bundle
import { setupLibs } from "./modules/libs-module";
setupLibs();

// Now use real lodash and date-fns
const _ = require("lodash");
const { format, isToday } = require("date-fns");
```

## React Patterns Used

### 1. useCallback for Event Handlers

Prevents child component re-renders by memoizing callbacks:

```javascript
const handleAddTask = useCallback((task) => {
  setTasks(prev => [...prev, task]);
}, []);
```

### 2. useMemo for Expensive Computations

Filters and sorts 100+ tasks only when dependencies change:

```javascript
const filteredTasks = useMemo(() => {
  let result = tasks;
  if (filter === "active") result = result.filter(t => !t.completed);
  if (sort === "dueDate") result = LIB.sortBy(result, "dueDate");
  return result;
}, [tasks, filter, sort]);
```

### 3. Component Composition

Breaking UI into focused components:

```javascript
<TaskForm onAddTask={handleAddTask} />
<FilterControls filter={filter} setFilter={setFilter} />
{tasks.map(task => <TaskItem key={task.id} task={task} />)}
```

### 4. Derived State

Calculate statistics from the main state:

```javascript
const stats = useMemo(() => ({
  total: tasks.length,
  active: tasks.filter(t => !t.completed).length,
  completed: tasks.filter(t => t.completed).length,
  overdue: tasks.filter(t => 
    !t.completed && DATE.differenceInDays(new Date(t.dueDate), new Date()) < 0
  ).length
}), [tasks]);
```

## Features Showcased

### Complex Filtering Logic
```javascript
if (filter === "overdue") {
  result = result.filter(t => 
    !t.completed && DATE.differenceInDays(new Date(t.dueDate), new Date()) < 0
  );
}
```

### Conditional Styling
```javascript
const statusColor = task.completed ? "#10b981" : isOverdue ? "#ef4444" : "#6366f1";
const style = { color: statusColor, /* ... */ };
```

### Form State Management
```javascript
const [title, setTitle] = useState("");
const [dueDate, setDueDate] = useState("");
const [priority, setPriority] = useState("medium");

const handleSubmit = (e) => {
  e.preventDefault();
  onAddTask({ id: Date.now(), title, dueDate, priority, completed: false });
  setTitle("");  // Reset form
};
```

### Immutable State Updates
```javascript
// Add task
setTasks(prev => [...prev, newTask]);

// Toggle task
setTasks(prev => prev.map(t => 
  t.id === id ? { ...t, completed: !t.completed } : t
));

// Delete task
setTasks(prev => prev.filter(t => t.id !== id));
```

## Extending This App

### Add More Features
1. **Local storage persistence**: Save tasks to file
2. **Categories/Tags**: Group tasks by category
3. **Recurring tasks**: Daily/weekly/monthly tasks
4. **Reminders**: Time-based notifications
5. **Task notes**: Add rich text descriptions
6. **Subtasks**: Break down complex tasks

### Integrate with Real Libraries

```javascript
// After bundling:
import { debounce } from "lodash";
import { format, distanceInWords } from "date-fns";

// Debounce search input
const handleSearch = useCallback(
  debounce((query) => filterTasks(query), 300),
  []
);

// Human-readable dates
const relativeDate = distanceInWords(task.dueDate, new Date());
```

### API Integration

```javascript
const { data: tasks, loading, error } = useFetch("/api/tasks");

// Save task to server
const handleAddTask = async (task) => {
  const response = await fetch("/api/tasks", {
    method: "POST",
    body: JSON.stringify(task)
  });
  const saved = await response.json();
  setTasks(prev => [...prev, saved]);
};
```

## Building for Production

### Optimize Bundle Size
```bash
# Current bundle includes all of lodash/date-fns
# Reduce by exporting only what you use in bundle-libs.js
```

### Add Type Safety
```typescript
interface Task {
  id: number;
  title: string;
  dueDate: string;
  priority: "high" | "medium" | "low";
  completed: boolean;
  createdAt: Date;
}
```

### Test Components
```javascript
// Isolate and test component logic
function testAddTask() {
  const tasks = [];
  handleAddTask({ id: 1, title: "Test" });
  assert(tasks.length === 1);
}
```

## Resources

- [React Documentation](https://react.dev) - Learn React
- [React Hooks Reference](https://react.dev/reference/react) - Hook APIs
- [Lodash Docs](https://lodash.com/docs) - Utility functions
- [date-fns Docs](https://date-fns.org) - Date utilities
- [art/react API](../../art/stdlib/react.ts) - Artisan React module
- [Bundling Guide](../../docs/BUNDLING.md) - Bundle npm packages

## Performance Tips

1. **Memoize computations**: Use useMemo for filters/sorts
2. **Memoize callbacks**: Use useCallback for event handlers
3. **Split components**: Smaller components = fewer re-renders
4. **Use keys**: Always use stable keys in lists
5. **Lazy load**: Defer loading heavy libraries

## Troubleshooting

### Tasks not updating?
Check that you're using immutable updates:
```javascript
// ✗ Wrong - mutates state
tasks[0].completed = true;

// ✓ Correct - creates new object
setTasks(prev => prev.map((t, i) => i === 0 ? {...t, completed: true} : t));
```

### Filtering/sorting slow?
Wrap in useMemo:
```javascript
const filtered = useMemo(() => {
  // expensive filtering
}, [tasks, filter]);
```

### App not rendering?
Ensure createRoot is called:
```javascript
const root = createRoot("root");
root.render(<App />);
```

---

Happy task managing! 🚀
