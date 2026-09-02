export function add(a: number, b: number): number { return a + b; }
export interface Point { x: number; y: number; }
export function distanceSquared(p: Point): number { return p.x * p.x + p.y * p.y; }

function helper(): number { return 1; } // not exported - private to this file
