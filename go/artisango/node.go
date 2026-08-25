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
	found := C.ArtisanNodeFindById(n.ptr, cid)
	if found == nil {
		return nil
	}
	return &Node{ptr: found}
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

// SetOnClick registers the handler Click() invokes on the C++ side -
// typically used for a <button>. Replaces (and releases) any handler
// already set on this node.
func (n Node) SetOnClick(fn func()) {
	handle := cgo.NewHandle(fn)
	C.ArtisanNodeSetOnClick(n.ptr, C.uintptr_t(handle))
}

// ArtisanGoInvokeClickHandler is called from C++ (node_c_api.cpp's
// GoCallback) when a node registered via SetOnClick is clicked - not
// meant to be called directly from Go app code.
//
//export ArtisanGoInvokeClickHandler
func ArtisanGoInvokeClickHandler(handle C.uintptr_t) {
	fn := cgo.Handle(handle).Value().(func())
	fn()
}

// ArtisanGoReleaseClickHandler is called from C++ when a click handler
// is replaced or its node destroyed, to release the handle SetOnClick
// created - not meant to be called directly from Go app code.
//
//export ArtisanGoReleaseClickHandler
func ArtisanGoReleaseClickHandler(handle C.uintptr_t) {
	cgo.Handle(handle).Delete()
}
