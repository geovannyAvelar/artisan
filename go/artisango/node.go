// Package artisango is the Go counterpart to SetupApp (C++) and the
// embedded script (JS): a thin, idiomatic wrapper over node_c_api.h, the
// C ABI artisan exposes so a Go app - compiled ahead of time into a
// static archive and linked into the same binary, see CMakeLists.txt's
// ARTISAN_APP_GO_SOURCE - can drive the same mutable DOM (dom_node.h)
// SetupApp and script both drive.
//
// A project's own Go app is a package main that imports artisango and
// exports ArtisanSetupApp - see the template artisan-cli's `component`-
// style scaffolding writes into a new project's goapp/main.go:
//
//	package main
//
//	import "C"
//	import (
//		"unsafe"
//
//		"artisango"
//	)
//
//	func SetupApp(doc artisango.Node) {
//		button := doc.FindById("my-button")
//		if button != nil {
//			button.SetOnClick(func() {
//				// ...
//			})
//		}
//	}
//
//	//export ArtisanSetupApp
//	func ArtisanSetupApp(doc unsafe.Pointer) {
//		SetupApp(artisango.WrapNode(doc))
//	}
//
//	func main() {}
package artisango

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#include "node_c_api.h"
#include <stdlib.h>
*/
import "C"

import (
	"runtime/cgo"
	"unsafe"
)

// Node wraps a live ArtisanNode* - never construct one directly; get one
// from WrapNode (the document, at the top of ArtisanSetupApp) or from
// another Node's FindById.
type Node struct {
	ptr *C.ArtisanNode
}

// NodeType values - see Node.NodeType.
const (
	ElementNode = C.ARTISAN_NODE_TYPE_ELEMENT
	TextNode    = C.ARTISAN_NODE_TYPE_TEXT
)

// NodeType is ElementNode or TextNode above.
func (n Node) NodeType() int {
	return int(C.ArtisanNodeType(n.ptr))
}

// WrapNode turns the raw pointer ArtisanSetupApp receives into a Node.
func WrapNode(ptr unsafe.Pointer) Node {
	return Node{ptr: (*C.ArtisanNode)(ptr)}
}

// FindById is a depth-first search of this subtree (including the node
// itself) for an element whose "id" attribute equals id. nil if none
// match.
func (n Node) FindById(id string) *Node {
	cid := C.CString(id)
	defer C.free(unsafe.Pointer(cid))
	return wrapOrNil(C.ArtisanNodeFindById(n.ptr, cid))
}

// TagName is "" for a text node, e.g. "div"/"button" for an element.
func (n Node) TagName() string {
	cstr := C.ArtisanNodeTagName(n.ptr)
	defer C.ArtisanFreeString(cstr)
	return C.GoString(cstr)
}

// TextContent is this node's own text (text node) or the concatenation
// of every descendant text node's content, depth-first (element).
func (n Node) TextContent() string {
	cstr := C.ArtisanNodeTextContent(n.ptr)
	defer C.ArtisanFreeString(cstr)
	return C.GoString(cstr)
}

// SetTextContent replaces a text node's own text, or - matching real DOM
// `.textContent =` semantics - discards an element's existing children
// and replaces them with a single new text node.
func (n Node) SetTextContent(text string) {
	ctext := C.CString(text)
	defer C.free(unsafe.Pointer(ctext))
	C.ArtisanNodeSetTextContent(n.ptr, ctext)
}

// GetAttribute reports the named attribute's value and whether it's set
// at all (an empty string and an unset attribute both report ok - check
// ok, not the string, to tell them apart).
func (n Node) GetAttribute(name string) (value string, ok bool) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	cvalue := C.ArtisanNodeGetAttribute(n.ptr, cname)
	if cvalue == nil {
		return "", false
	}
	defer C.ArtisanFreeString(cvalue)
	return C.GoString(cvalue), true
}

func (n Node) SetAttribute(name, value string) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	cvalue := C.CString(value)
	defer C.free(unsafe.Pointer(cvalue))
	C.ArtisanNodeSetAttribute(n.ptr, cname, cvalue)
}

func (n Node) RemoveAttribute(name string) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	C.ArtisanNodeRemoveAttribute(n.ptr, cname)
}

// HasAttribute reports whether the named attribute is set at all,
// regardless of its value (including "").
func (n Node) HasAttribute(name string) bool {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	return bool(C.ArtisanNodeHasAttribute(n.ptr, cname))
}

// ParentNode, NextSibling, and PreviousSibling are nil if there's no
// parent/no such sibling - see the matching Node methods (dom_node.h).
func (n Node) ParentNode() *Node {
	return wrapOrNil(C.ArtisanNodeParentNode(n.ptr))
}

func (n Node) NextSibling() *Node {
	return wrapOrNil(C.ArtisanNodeNextSibling(n.ptr))
}

func (n Node) PreviousSibling() *Node {
	return wrapOrNil(C.ArtisanNodePreviousSibling(n.ptr))
}

// Children is a snapshot of this node's children at call time, not a
// live view - the same simplification document.querySelectorAll's Go/JS
// counterparts already make (see css.h's QuerySelector doc comment).
func (n Node) Children() []Node {
	count := int(C.ArtisanNodeChildCount(n.ptr))
	children := make([]Node, count)
	for i := 0; i < count; i++ {
		children[i] = Node{ptr: C.ArtisanNodeChildAt(n.ptr, C.size_t(i))}
	}
	return children
}

// QuerySelector/QuerySelectorAll search this node's subtree (not
// including the node itself) for element(s) matching selector - the same
// bounded grammar documented on css.h's Selector (one compound selector,
// no combinators/comma-lists). QuerySelector is nil, and
// QuerySelectorAll is an empty (never nil) slice, if nothing matches.
func (n Node) QuerySelector(selector string) *Node {
	cselector := C.CString(selector)
	defer C.free(unsafe.Pointer(cselector))
	return wrapOrNil(C.ArtisanQuerySelector(n.ptr, cselector))
}

func (n Node) QuerySelectorAll(selector string) []Node {
	cselector := C.CString(selector)
	defer C.free(unsafe.Pointer(cselector))
	var count C.size_t
	arr := C.ArtisanQuerySelectorAll(n.ptr, cselector, &count)
	if arr == nil {
		return []Node{}
	}
	defer C.ArtisanFreeNodeArray(arr)
	ptrs := unsafe.Slice(arr, int(count))
	found := make([]Node, int(count))
	for i, ptr := range ptrs {
		found[i] = Node{ptr: ptr}
	}
	return found
}

// Matches reports whether n itself matches selector - same bounded
// grammar as QuerySelector above.
func (n Node) Matches(selector string) bool {
	cselector := C.CString(selector)
	defer C.free(unsafe.Pointer(cselector))
	return bool(C.ArtisanNodeMatches(n.ptr, cselector))
}

// Closest is n, or its nearest ancestor (inclusive), that matches
// selector - nil if neither does.
func (n Node) Closest(selector string) *Node {
	cselector := C.CString(selector)
	defer C.free(unsafe.Pointer(cselector))
	return wrapOrNil(C.ArtisanNodeClosest(n.ptr, cselector))
}

// wrapOrNil turns a possibly-nullptr ArtisanNode* into a possibly-nil
// *Node - the shared "found nothing" shape FindById/ParentNode/
// NextSibling/PreviousSibling/QuerySelector all return.
func wrapOrNil(ptr *C.ArtisanNode) *Node {
	if ptr == nil {
		return nil
	}
	return &Node{ptr: ptr}
}

// SetOnClick registers the handler Click() invokes on the C++ side -
// typically used for a <button>. Replaces (and releases) any handler
// already set on this node.
func (n Node) SetOnClick(fn func()) {
	handle := cgo.NewHandle(fn)
	C.ArtisanNodeSetOnClick(n.ptr, C.uintptr_t(handle))
}

// ListenerHandle identifies one AddEventListener registration, for a
// later RemoveEventListener call - Go func values aren't comparable the
// way JS function references are (real addEventListener/
// removeEventListener pass the same fn to both), so removal instead
// keys off of this token, the same cgo.Handle node_c_api.h's
// ArtisanNodeAddEventListener/RemoveEventListener already use for
// identity underneath.
type ListenerHandle uintptr

// AddEventListener registers fn for eventType, alongside (not replacing)
// any other handler already registered for that type or any other -
// unlike SetOnClick above, matching real addEventListener. Any type
// string works; "click", "change" (checkbox/radio), and "input" (text
// fields) fire on their own (see main.cpp), and DispatchEvent below
// fires any other type. capture: fires during the capturing phase
// instead of the bubbling phase - see DispatchEvent. The returned handle
// is only needed if you plan to RemoveEventListener this fn later.
func (n Node) AddEventListener(eventType string, fn func(), capture bool) ListenerHandle {
	ceventType := C.CString(eventType)
	defer C.free(unsafe.Pointer(ceventType))
	handle := cgo.NewHandle(fn)
	C.ArtisanNodeAddEventListener(n.ptr, ceventType, C.uintptr_t(handle), C.bool(capture))
	return ListenerHandle(handle)
}

// RemoveEventListener removes the listener AddEventListener registered
// and returned handle for - eventType/capture must match the original
// AddEventListener call. A handle that matches nothing (already
// removed, or from a different node) is a safe no-op.
func (n Node) RemoveEventListener(eventType string, handle ListenerHandle, capture bool) {
	ceventType := C.CString(eventType)
	defer C.free(unsafe.Pointer(ceventType))
	C.ArtisanNodeRemoveEventListener(n.ptr, ceventType, C.uintptr_t(handle), C.bool(capture))
}

// DispatchEvent fires eventType at n through the usual capturing/target/
// bubbling walk - every listener registered for it runs, whether
// registered from Go (AddEventListener/SetOnClick) or JS
// (addEventListener) on the same node. bubbles = false skips the
// ancestor phases; cancelable = false makes any listener's
// preventDefault() (JS-side only - a Go listener's fn is zero-arg, so it
// has no way to call it) a no-op. Returns false if the event was
// cancelable and some listener called preventDefault(), true otherwise.
func (n Node) DispatchEvent(eventType string, bubbles, cancelable bool) bool {
	ceventType := C.CString(eventType)
	defer C.free(unsafe.Pointer(ceventType))
	return bool(C.ArtisanNodeDispatchEvent(n.ptr, ceventType, C.bool(bubbles), C.bool(cancelable)))
}

// CreateElement/CreateTextNode create a detached element/text node - not
// yet part of any tree. Append it somewhere (AppendChild/InsertBefore
// below) before this Node value goes out of scope: the underlying C++
// object is only freed once actually attached, or never, if it's
// abandoned instead - there's no Go-side finalizer to catch that the way
// a garbage-collected JS wrapper would (see ArtisanCreateElement's doc
// comment in node_c_api.h).
func CreateElement(tag string) Node {
	ctag := C.CString(tag)
	defer C.free(unsafe.Pointer(ctag))
	return Node{ptr: C.ArtisanCreateElement(ctag)}
}

func CreateTextNode(text string) Node {
	ctext := C.CString(text)
	defer C.free(unsafe.Pointer(ctext))
	return Node{ptr: C.ArtisanCreateTextNode(ctext)}
}

// AppendChild takes ownership of a node created via CreateElement/
// CreateTextNode, appending it after this node's existing children.
// Passing a node that isn't currently a pending, unattached
// CreateElement/CreateTextNode result - one already attached elsewhere,
// say - is a safe no-op (re-parenting isn't supported, same restriction
// the JS binding enforces, there by throwing; there's no exception
// mechanism to report that here). Returns child back, for chaining.
func (n Node) AppendChild(child Node) Node {
	C.ArtisanNodeAppendChild(n.ptr, child.ptr)
	return child
}

// InsertBefore is AppendChild's more general form: inserts child
// directly before the given sibling instead of at the end. before == nil
// means append at the end, matching real DOM insertBefore(node, null).
func (n Node) InsertBefore(child Node, before *Node) Node {
	var cbefore *C.ArtisanNode
	if before != nil {
		cbefore = before.ptr
	}
	C.ArtisanNodeInsertBefore(n.ptr, child.ptr, cbefore)
	return child
}

// RemoveChild detaches child (still alive and re-appendable, in the same
// pending state CreateElement/CreateTextNode's result starts in - see
// their doc comment) and returns it, or nil if child isn't actually a
// child of n.
func (n Node) RemoveChild(child Node) *Node {
	return wrapOrNil(C.ArtisanNodeRemoveChild(n.ptr, child.ptr))
}

// Remove detaches n from its parent and returns it in that same pending
// state, or nil if it had no parent.
func (n Node) Remove() *Node {
	return wrapOrNil(C.ArtisanNodeRemove(n.ptr))
}

// CloneNode is a new, detached, pending copy of n - same tag/text/
// attributes, never event listeners. deep also clones every descendant.
func (n Node) CloneNode(deep bool) Node {
	return Node{ptr: C.ArtisanNodeCloneNode(n.ptr, C.bool(deep))}
}

// ClassList is a live view over n's "class" attribute tokens - each
// method re-reads/re-writes the attribute directly, so it can't drift
// from it.
type ClassList struct {
	node Node
}

func (n Node) ClassList() ClassList {
	return ClassList{node: n}
}

func (c ClassList) Add(name string) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	C.ArtisanNodeClassListAdd(c.node.ptr, cname)
}

func (c ClassList) Remove(name string) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	C.ArtisanNodeClassListRemove(c.node.ptr, cname)
}

func (c ClassList) Contains(name string) bool {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	return bool(C.ArtisanNodeClassListContains(c.node.ptr, cname))
}

// Toggle adds name if absent, removes it if present, and returns the
// resulting membership.
func (c ClassList) Toggle(name string) bool {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	return bool(C.ArtisanNodeClassListToggle(c.node.ptr, cname, C.bool(false), C.bool(false)))
}

// ToggleForce pins name's membership to force instead of toggling it -
// real classList.toggle(name, force) semantics.
func (c ClassList) ToggleForce(name string, force bool) bool {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	return bool(C.ArtisanNodeClassListToggle(c.node.ptr, cname, C.bool(true), C.bool(force)))
}

// Style is a live view over n's inline style="..." attribute - exactly
// the five properties [Using CSS] in the README supports (color/
// backgroundColor/fontWeight/borderColor/borderWidth).
type Style struct {
	node Node
}

func (n Node) Style() Style {
	return Style{node: n}
}

// Get reports one property's raw value and whether it's set at all.
func (s Style) Get(property string) (value string, ok bool) {
	cproperty := C.CString(property)
	defer C.free(unsafe.Pointer(cproperty))
	cvalue := C.ArtisanNodeStyleGet(s.node.ptr, cproperty)
	if cvalue == nil {
		return "", false
	}
	defer C.ArtisanFreeString(cvalue)
	return C.GoString(cvalue), true
}

// Set assigns one property - an empty value removes it instead, matching
// real element.style.color = "".
func (s Style) Set(property, value string) {
	cproperty := C.CString(property)
	defer C.free(unsafe.Pointer(cproperty))
	cvalue := C.CString(value)
	defer C.free(unsafe.Pointer(cvalue))
	C.ArtisanNodeStyleSet(s.node.ptr, cproperty, cvalue)
}

// GetData/SetData are dataset's data-* convention, method-based rather
// than true dataset.fooBar property syntax - "fooBar" <-> "data-foo-bar".
// GetData reports the value and whether it's set at all.
func (n Node) GetData(name string) (value string, ok bool) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	cvalue := C.ArtisanNodeGetData(n.ptr, cname)
	if cvalue == nil {
		return "", false
	}
	defer C.ArtisanFreeString(cvalue)
	return C.GoString(cvalue), true
}

func (n Node) SetData(name, value string) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	cvalue := C.CString(value)
	defer C.free(unsafe.Pointer(cvalue))
	C.ArtisanNodeSetData(n.ptr, cname, cvalue)
}

// ArtisanGoInvokeHandler is called from C++ (node_c_api.cpp's
// GoCallback) when a node registered via SetOnClick or AddEventListener
// fires - not meant to be called directly from Go app code.
//
//export ArtisanGoInvokeHandler
func ArtisanGoInvokeHandler(handle C.uintptr_t) {
	fn := cgo.Handle(handle).Value().(func())
	fn()
}

// ArtisanGoReleaseHandler is called from C++ when a handler is replaced
// or its node destroyed, to release the handle SetOnClick/
// AddEventListener created - not meant to be called directly from Go app
// code.
//
//export ArtisanGoReleaseHandler
func ArtisanGoReleaseHandler(handle C.uintptr_t) {
	cgo.Handle(handle).Delete()
}
