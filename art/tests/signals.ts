// The Signal<T>/effect/makeSignal reactive pattern (README.md's
// "Signals" section) - built from generics, get/set accessors,
// non-literal globals, indirect calls, and makeArray<T>. Self-contained,
// no DOM involved.

function noopEffect(): void {}

let currentEffect: () => void = noopEffect;

class Signal<T> {
  raw: T;
  subscribers: () => void[];
  subscriberCount: number;
  capacity: number;

  get value(): T {
    if (currentEffect != noopEffect) { this.subscribe(currentEffect); }
    return this.raw;
  }
  set value(v: T) {
    this.raw = v;
    this.notify();
  }

  function subscribe(fn: () => void): void {
    let i: number = 0;
    while (i < this.subscriberCount) {
      if (this.subscribers[i] == fn) { return; }
      i = i + 1;
    }
    if (this.subscriberCount == this.capacity) { this.grow(); }
    this.subscribers[this.subscriberCount] = fn;
    this.subscriberCount = this.subscriberCount + 1;
  }

  function unsubscribe(fn: () => void): void {
    let i: number = 0;
    while (i < this.subscriberCount) {
      if (this.subscribers[i] == fn) {
        let j: number = i;
        while (j < this.subscriberCount - 1) {
          this.subscribers[j] = this.subscribers[j + 1];
          j = j + 1;
        }
        this.subscriberCount = this.subscriberCount - 1;
        this.subscribers[this.subscriberCount] = noopEffect;
        return;
      }
      i = i + 1;
    }
  }

  function grow(): void {
    let newCapacity: number = this.capacity * 2;
    let newSubscribers: () => void[] = makeArray::<() => void>(newCapacity, noopEffect);
    let i: number = 0;
    while (i < this.subscriberCount) {
      newSubscribers[i] = this.subscribers[i];
      i = i + 1;
    }
    this.subscribers = newSubscribers;
    this.capacity = newCapacity;
  }

  function notify(): void {
    let i: number = 0;
    while (i < this.subscriberCount) {
      let fn: () => void = this.subscribers[i];
      fn();
      i = i + 1;
    }
  }
}

function makeSignal<T>(initial: T): Signal<T> {
  return { raw: initial, subscriberCount: 0, capacity: 4, subscribers: makeArray::<() => void>(4, noopEffect) };
}

function effect(fn: () => void): void {
  let saved: () => void = currentEffect;
  currentEffect = fn;
  fn();
  currentEffect = saved;
}

let counter: Signal<number> = makeSignal::<number>(0);
let runs: number = 0;

function onCounterChange(): void {
  let v: number = counter.value;
  runs = runs + 1;
}

function main(): number {
  let fails: number = 0;

  effect(onCounterChange);
  if (runs != 1) { fails = fails + 1; } // ran once immediately

  counter.value = 1;
  if (runs != 2) { fails = fails + 1; } // write re-ran the effect

  counter.value = 2;
  if (runs != 3) { fails = fails + 1; }

  counter.unsubscribe(onCounterChange);
  counter.value = 3;
  if (runs != 3) { fails = fails + 1; } // unsubscribed - no more runs

  return fails;
}
