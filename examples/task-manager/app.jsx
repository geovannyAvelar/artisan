// Task Manager - Complex React App with Bundled Libraries
// Demonstrates: React hooks, forms, state management, component composition
// Uses bundled libraries: lodash and date-fns

import { setupReact, createRoot, useState, useEffect, useCallback, useMemo, useRef } from "art/react";

setupReact();

// Mock bundled libraries (in production, would come from bundles/libs.js)
// For now, we'll use simple JavaScript equivalents
const LIB = {
  sortBy: (arr, key) => [...arr].sort((a, b) => {
    if (typeof a[key] === "string") return a[key].localeCompare(b[key]);
    return a[key] - b[key];
  }),
  filter: (arr, fn) => arr.filter(fn),
  map: (arr, fn) => arr.map(fn),
  groupBy: (arr, key) => {
    const groups = {};
    arr.forEach(item => {
      const k = item[key];
      if (!groups[k]) groups[k] = [];
      groups[k].push(item);
    });
    return groups;
  },
  find: (arr, fn) => arr.find(fn),
  uniq: (arr) => [...new Set(arr)],
  isEmpty: (obj) => Object.keys(obj).length === 0,
  debounce: (fn, delay) => {
    let timeout;
    return function(...args) {
      clearTimeout(timeout);
      timeout = setTimeout(() => fn(...args), delay);
    };
  }
};

// Date utilities (simplified date-fns replacement)
const DATE = {
  format: (date, fmt) => {
    const d = new Date(date);
    return d.toLocaleDateString() + " " + d.toLocaleTimeString();
  },
  isToday: (date) => {
    const d = new Date(date);
    const today = new Date();
    return d.toDateString() === today.toDateString();
  },
  isTomorrow: (date) => {
    const d = new Date(date);
    const tomorrow = new Date();
    tomorrow.setDate(tomorrow.getDate() + 1);
    return d.toDateString() === tomorrow.toDateString();
  },
  addDays: (date, days) => {
    const d = new Date(date);
    d.setDate(d.getDate() + days);
    return d;
  },
  differenceInDays: (d1, d2) => {
    const ms = new Date(d1) - new Date(d2);
    return Math.floor(ms / (1000 * 60 * 60 * 24));
  }
};

// Task form component
function TaskForm({ onAddTask }) {
  const [title, setTitle] = useState("");
  const [dueDate, setDueDate] = useState("");
  const [priority, setPriority] = useState("medium");

  const handleSubmit = useCallback((e) => {
    e.preventDefault();
    if (title.trim()) {
      onAddTask({
        id: Date.now(),
        title,
        dueDate: dueDate || new Date().toISOString(),
        priority,
        completed: false,
        createdAt: new Date()
      });
      setTitle("");
      setDueDate("");
      setPriority("medium");
    }
  }, [title, dueDate, priority, onAddTask]);

  return (
    <form onSubmit={handleSubmit} style={{
      background: "white",
      padding: "20px",
      borderRadius: "10px",
      marginBottom: "20px",
      boxShadow: "0 4px 6px rgba(0, 0, 0, 0.1)"
    }}>
      <h2 style={{ marginBottom: "15px", color: "#333" }}>Add New Task</h2>

      <div style={{ marginBottom: "15px" }}>
        <label style={{ display: "block", marginBottom: "5px", fontWeight: "600", color: "#555" }}>
          Task Title
        </label>
        <input
          type="text"
          value={title}
          onChange={(e) => setTitle(e.target.value)}
          placeholder="Enter task description..."
          style={{
            width: "100%",
            padding: "10px",
            border: "1px solid #ddd",
            borderRadius: "5px",
            fontSize: "14px",
            fontFamily: "inherit"
          }}
        />
      </div>

      <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "15px", marginBottom: "15px" }}>
        <div>
          <label style={{ display: "block", marginBottom: "5px", fontWeight: "600", color: "#555" }}>
            Due Date
          </label>
          <input
            type="date"
            value={dueDate}
            onChange={(e) => setDueDate(e.target.value)}
            style={{
              width: "100%",
              padding: "10px",
              border: "1px solid #ddd",
              borderRadius: "5px",
              fontSize: "14px"
            }}
          />
        </div>

        <div>
          <label style={{ display: "block", marginBottom: "5px", fontWeight: "600", color: "#555" }}>
            Priority
          </label>
          <select
            value={priority}
            onChange={(e) => setPriority(e.target.value)}
            style={{
              width: "100%",
              padding: "10px",
              border: "1px solid #ddd",
              borderRadius: "5px",
              fontSize: "14px"
            }}
          >
            <option value="low">Low</option>
            <option value="medium">Medium</option>
            <option value="high">High</option>
          </select>
        </div>
      </div>

      <button
        type="submit"
        style={{
          width: "100%",
          padding: "12px",
          background: "linear-gradient(135deg, #667eea 0%, #764ba2 100%)",
          color: "white",
          border: "none",
          borderRadius: "5px",
          fontSize: "16px",
          fontWeight: "600",
          cursor: "pointer",
          transition: "transform 0.2s"
        }}
        onMouseEnter={(e) => e.target.style.transform = "scale(1.02)"}
        onMouseLeave={(e) => e.target.style.transform = "scale(1)"}
      >
        Add Task
      </button>
    </form>
  );
}

// Task item component
function TaskItem({ task, onToggle, onDelete }) {
  const daysLeft = DATE.differenceInDays(new Date(task.dueDate), new Date());
  const isOverdue = daysLeft < 0 && !task.completed;
  const isToday = DATE.isToday(task.dueDate);

  const priorityColor = {
    high: "#f87171",
    medium: "#fbbf24",
    low: "#60a5fa"
  };

  const statusColor = task.completed ? "#10b981" : isOverdue ? "#ef4444" : "#6366f1";

  return (
    <div style={{
      background: "white",
      padding: "15px",
      borderRadius: "8px",
      marginBottom: "10px",
      display: "flex",
      alignItems: "center",
      gap: "15px",
      borderLeft: `4px solid ${priorityColor[task.priority]}`,
      boxShadow: "0 2px 4px rgba(0, 0, 0, 0.1)",
      opacity: task.completed ? 0.7 : 1
    }}>
      <input
        type="checkbox"
        checked={task.completed}
        onChange={() => onToggle(task.id)}
        style={{
          width: "20px",
          height: "20px",
          cursor: "pointer"
        }}
      />

      <div style={{ flex: 1 }}>
        <div style={{
          fontSize: "16px",
          fontWeight: "500",
          color: task.completed ? "#999" : "#333",
          textDecoration: task.completed ? "line-through" : "none"
        }}>
          {task.title}
        </div>

        <div style={{
          fontSize: "13px",
          color: "#666",
          marginTop: "5px",
          display: "flex",
          gap: "15px"
        }}>
          <span>
            📅 {new Date(task.dueDate).toLocaleDateString()}
            {isToday && " (Today)"}
            {isOverdue && ` (${Math.abs(daysLeft)} days overdue)`}
          </span>
          <span style={{ color: priorityColor[task.priority], fontWeight: "600" }}>
            {task.priority.toUpperCase()}
          </span>
        </div>
      </div>

      <button
        onClick={() => onDelete(task.id)}
        style={{
          padding: "8px 12px",
          background: "#ef4444",
          color: "white",
          border: "none",
          borderRadius: "5px",
          cursor: "pointer",
          fontSize: "13px"
        }}
      >
        Delete
      </button>
    </div>
  );
}

// Filter and sort controls
function FilterControls({ filter, setFilter, sort, setSort, stats }) {
  return (
    <div style={{
      background: "white",
      padding: "15px",
      borderRadius: "10px",
      marginBottom: "20px",
      display: "grid",
      gridTemplateColumns: "repeat(auto-fit, minmax(200px, 1fr))",
      gap: "15px",
      boxShadow: "0 4px 6px rgba(0, 0, 0, 0.1)"
    }}>
      <div>
        <label style={{ display: "block", marginBottom: "5px", fontWeight: "600", color: "#555" }}>
          Filter
        </label>
        <select
          value={filter}
          onChange={(e) => setFilter(e.target.value)}
          style={{
            width: "100%",
            padding: "10px",
            border: "1px solid #ddd",
            borderRadius: "5px"
          }}
        >
          <option value="all">All Tasks ({stats.total})</option>
          <option value="active">Active ({stats.active})</option>
          <option value="completed">Completed ({stats.completed})</option>
          <option value="overdue">Overdue ({stats.overdue})</option>
        </select>
      </div>

      <div>
        <label style={{ display: "block", marginBottom: "5px", fontWeight: "600", color: "#555" }}>
          Sort By
        </label>
        <select
          value={sort}
          onChange={(e) => setSort(e.target.value)}
          style={{
            width: "100%",
            padding: "10px",
            border: "1px solid #ddd",
            borderRadius: "5px"
          }}
        >
          <option value="dueDate">Due Date</option>
          <option value="priority">Priority</option>
          <option value="created">Recently Created</option>
        </select>
      </div>

      <div style={{
        display: "flex",
        alignItems: "flex-end",
        gap: "10px",
        padding: "0 10px"
      }}>
        <div>
          <div style={{ fontSize: "12px", color: "#888" }}>Completion Rate</div>
          <div style={{ fontSize: "20px", fontWeight: "bold", color: "#667eea" }}>
            {stats.total === 0 ? "0%" : Math.round((stats.completed / stats.total) * 100) + "%"}
          </div>
        </div>
      </div>
    </div>
  );
}

// Main App component
function App() {
  const [tasks, setTasks] = useState([
    {
      id: 1,
      title: "Review project proposal",
      dueDate: new Date().toISOString(),
      priority: "high",
      completed: false,
      createdAt: new Date()
    },
    {
      id: 2,
      title: "Update documentation",
      dueDate: DATE.addDays(new Date(), 2).toISOString(),
      priority: "medium",
      completed: false,
      createdAt: new Date()
    }
  ]);

  const [filter, setFilter] = useState("all");
  const [sort, setSort] = useState("dueDate");

  const handleAddTask = useCallback((task) => {
    setTasks(prev => [...prev, task]);
  }, []);

  const handleToggle = useCallback((id) => {
    setTasks(prev => prev.map(t =>
      t.id === id ? { ...t, completed: !t.completed } : t
    ));
  }, []);

  const handleDelete = useCallback((id) => {
    setTasks(prev => prev.filter(t => t.id !== id));
  }, []);

  // Filter and sort tasks
  const filteredTasks = useMemo(() => {
    let result = tasks;

    // Apply filter
    if (filter === "active") {
      result = result.filter(t => !t.completed);
    } else if (filter === "completed") {
      result = result.filter(t => t.completed);
    } else if (filter === "overdue") {
      result = result.filter(t =>
        !t.completed && DATE.differenceInDays(new Date(t.dueDate), new Date()) < 0
      );
    }

    // Apply sort
    if (sort === "dueDate") {
      result = LIB.sortBy(result, "dueDate");
    } else if (sort === "priority") {
      const priorityOrder = { high: 0, medium: 1, low: 2 };
      result.sort((a, b) => priorityOrder[a.priority] - priorityOrder[b.priority]);
    } else if (sort === "created") {
      result = result.reverse();
    }

    return result;
  }, [tasks, filter, sort]);

  // Calculate statistics
  const stats = useMemo(() => ({
    total: tasks.length,
    active: tasks.filter(t => !t.completed).length,
    completed: tasks.filter(t => t.completed).length,
    overdue: tasks.filter(t =>
      !t.completed && DATE.differenceInDays(new Date(t.dueDate), new Date()) < 0
    ).length
  }), [tasks]);

  return (
    <div style={{ minHeight: "100vh", paddingBottom: "40px" }}>
      <div style={{ maxWidth: "800px", margin: "0 auto" }}>
        <div style={{
          textAlign: "center",
          color: "white",
          marginBottom: "30px",
          marginTop: "20px"
        }}>
          <h1 style={{ fontSize: "32px", marginBottom: "10px" }}>📋 Task Manager</h1>
          <p style={{ fontSize: "16px", opacity: 0.9 }}>
            Built with React + Bundled Libraries (lodash, date-fns)
          </p>
        </div>

        <TaskForm onAddTask={handleAddTask} />
        <FilterControls filter={filter} setFilter={setFilter} sort={sort} setSort={setSort} stats={stats} />

        <div>
          {filteredTasks.length === 0 ? (
            <div style={{
              background: "white",
              padding: "40px",
              textAlign: "center",
              borderRadius: "10px",
              color: "#999"
            }}>
              <p style={{ fontSize: "16px" }}>No tasks yet. Create one to get started! 🚀</p>
            </div>
          ) : (
            filteredTasks.map(task => (
              <TaskItem
                key={task.id}
                task={task}
                onToggle={handleToggle}
                onDelete={handleDelete}
              />
            ))
          )}
        </div>
      </div>
    </div>
  );
}

// Mount the app
const root = createRoot("root");
root.render(<App />);
