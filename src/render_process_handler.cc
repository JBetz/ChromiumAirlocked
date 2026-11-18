// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <optional>

#include "render_process_handler.h"

#include <include/cef_parser.h>
#include <map>
#include <string>
#include "include/base/cef_logging.h"
#include "util.hpp"

const char kOnFocusMessage[] = "RenderProcessHandler.OnFocus";
const char kOnFocusOutMessage[] = "RenderProcessHandler.OnFocusOut";
const char kOnMouseOverMessage[] = "RenderProcessHandler.OnMouseOver";
const char kOnHistoryMessage[] = "RenderProcessHandler.OnHistory";
const char kOnNavigationMessage[] = "RenderProcessHandler.OnNavigation";
const char kOnEvalMessage[] = "RenderProcessHandler.OnEval";

RenderProcessHandler::RenderProcessHandler() {}

void RenderProcessHandler::SendLogMessage(CefRefPtr<CefFrame> frame,
                                          const cef_log_severity_t severity,
                                          const CefString& message) {
  if (!frame)
    return;

  if (severity == LOGSEVERITY_DEFAULT || severity == LOGSEVERITY_VERBOSE ||
      severity == LOGSEVERITY_INFO) {
    LOG(INFO) << message;
  } else if (severity == LOGSEVERITY_WARNING) {
    LOG(WARNING) << message;
  } else if (severity == LOGSEVERITY_ERROR) {
    LOG(ERROR) << message;
  } else if (severity == LOGSEVERITY_FATAL) {
    LOG(FATAL) << message;
  }

  // Package as an OnLogMessage notification to the host
  auto request = CefDictionaryValue::Create();
  request->SetString("id", uuid_to_string(CreateUuid()));
  request->SetString("class", "Browser");
  request->SetString("method", "OnLogMessage");
  request->SetInt("instanceId", frame->GetBrowser()->GetIdentifier());

  auto arguments = CefDictionaryValue::Create();
  arguments->SetInt("severity", severity);
  arguments->SetString("message", message);
  arguments->SetString("process", "Renderer");
  request->SetDictionary("arguments", arguments);

  auto json = CefValue::Create();
  json->SetDictionary(request);

  auto processMessage =
      CefProcessMessage::Create("RenderProcessHandler.OnLogMessage");
  processMessage->GetArgumentList()->SetValue(0, json);
  frame->SendProcessMessage(PID_BROWSER, processMessage);
}

void RenderProcessHandler::SendNavigationEvent(CefRefPtr<CefFrame> frame,
                                               const NavigationEvent& event) {
  CefRefPtr<CefDictionaryValue> request = CefDictionaryValue::Create();
  request->SetString("id", uuid_to_string(CreateUuid()));
  request->SetString("class", "Browser");
  request->SetString("method", "OnNavigated");
  request->SetInt("instanceId", frame->GetBrowser()->GetIdentifier());

  CefRefPtr<CefDictionaryValue> arguments = CefDictionaryValue::Create();

  CefRefPtr<CefDictionaryValue> sourceDict = CefDictionaryValue::Create();
  sourceDict->SetString("frameId", event.source.frameId);
  sourceDict->SetBool("isMainFrame", event.source.isMainFrame);
  arguments->SetDictionary("source", sourceDict);

  CefRefPtr<CefDictionaryValue> transitionDict = CefDictionaryValue::Create();
  transitionDict->SetString("type", event.transition.type);
  transitionDict->SetBool("userInitiated", event.transition.userInitiated);
  transitionDict->SetBool("hashChange", event.transition.hashChange);
  if (event.transition.info) {
    transitionDict->SetValue("info", event.transition.info);
  } else {
    transitionDict->SetNull("info");
  }
  arguments->SetDictionary("transition", transitionDict);

  CefRefPtr<CefDictionaryValue> destination = CefDictionaryValue::Create();
  destination->SetString("url", event.destination.url);
  destination->SetBool("sameDocument", event.destination.sameDocument);
  destination->SetString("key", event.destination.key);
  destination->SetString("id", event.destination.id);
  destination->SetInt("index", event.destination.index);
  if (event.destination.state) {
    destination->SetValue("state", event.destination.state);
  } else {
      destination->SetNull("state");
  }
  arguments->SetDictionary("destination", destination);
  request->SetDictionary("arguments", arguments);

  CefRefPtr<CefValue> json = CefValue::Create();
  json->SetDictionary(request);
  CefRefPtr<CefProcessMessage> message =
      CefProcessMessage::Create(kOnNavigationMessage);
  message->GetArgumentList()->SetValue(0, json);
  frame->SendProcessMessage(PID_BROWSER, message);
}

void RenderProcessHandler::SendTraverseRequest(
    CefRefPtr<CefFrame> frame,
    const TraverseRequest& request_) {
  CefRefPtr<CefDictionaryValue> request = CefDictionaryValue::Create();
  request->SetString("id", uuid_to_string(CreateUuid()));
  request->SetString("class", "Browser");
  request->SetString("method", "OnTraverseRequest");
  request->SetInt("instanceId", frame->GetBrowser()->GetIdentifier());

  CefRefPtr<CefDictionaryValue> arguments = CefDictionaryValue::Create();
  CefRefPtr<CefDictionaryValue> sourceDict = CefDictionaryValue::Create();
  sourceDict->SetString("frameId", request_.source.frameId);
  sourceDict->SetBool("isMainFrame", request_.source.isMainFrame);
  arguments->SetDictionary("source", sourceDict);
  arguments->SetInt("delta", request_.delta);
  arguments->SetString("key", request_.key);
  if (request_.info) {
    arguments->SetValue("info", request_.info);
  } else {
    arguments->SetNull("info");
  }
  request->SetDictionary("arguments", arguments);

  CefRefPtr<CefValue> json = CefValue::Create();
  json->SetDictionary(request);
  CefRefPtr<CefProcessMessage> message =
      CefProcessMessage::Create(kOnNavigationMessage);
  message->GetArgumentList()->SetValue(0, json);
  frame->SendProcessMessage(PID_BROWSER, message);
}

CefRefPtr<CefRenderProcessHandler>
RenderProcessHandler::GetRenderProcessHandler() {
  return this;
}

void RenderProcessHandler::OnWebKitInitialized() {}

void RenderProcessHandler::OnBrowserCreated(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefDictionaryValue> extra_info) {}

void RenderProcessHandler::OnBrowserDestroyed(CefRefPtr<CefBrowser> browser) {}

class MouseOverHandler : public CefV8Handler {
 public:
  MouseOverHandler() {}

  virtual bool Execute(const CefString& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception) override {
    CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
    CefRefPtr<CefFrame> frame = context->GetFrame();
    CefRefPtr<CefV8Value> event = arguments.front();
    CefV8ValueList stopPropagationArguments;
    event->GetValue("stopPropagation")
        ->ExecuteFunction(event, stopPropagationArguments);
    CefRefPtr<CefV8Value> target = event->GetValue("target");
    CefRefPtr<CefDictionaryValue> request = CefDictionaryValue::Create();
    request->SetString("id", uuid_to_string(CreateUuid()));
    request->SetString("class", "Browser");
    request->SetString("method", "OnMouseOver");
    request->SetInt("instanceId", frame->GetBrowser()->GetIdentifier());
    CefRefPtr<CefDictionaryValue> requestArguments =
        CefDictionaryValue::Create();
    std::string tagName = target->GetValue("tagName")->GetStringValue();
    requestArguments->SetString("tagName", tagName);
    if (tagName == "INPUT") {
      requestArguments->SetString("inputType",
                                  target->GetValue("type")->GetStringValue());
    }
    CefV8ValueList closestArguments;
    closestArguments.push_back(CefV8Value::CreateString("a"));
    CefRefPtr<CefV8Value> closestResult =
        target->GetValue("closest")->ExecuteFunction(target, closestArguments);
    CefV8ValueList getBoundingClientRectArguments;
    CefRefPtr<CefV8Value> v8Rectangle;
    if (closestResult->IsObject()) {
      std::string href = closestResult->GetValue("href")->GetStringValue();
      if (!href.empty()) {
        request->SetString("href", href);
      }
      v8Rectangle =
          closestResult->GetValue("getBoundingClientRect")
              ->ExecuteFunction(closestResult, getBoundingClientRectArguments);
    } else {
      v8Rectangle =
          target->GetValue("getBoundingClientRect")
              ->ExecuteFunction(target, getBoundingClientRectArguments);
    }
    if (v8Rectangle->IsObject()) {
      CefRefPtr<CefDictionaryValue> rectangle = CefDictionaryValue::Create();
      rectangle->SetInt("x", v8Rectangle->GetValue("x")->GetIntValue());
      rectangle->SetInt("y", v8Rectangle->GetValue("y")->GetIntValue());
      rectangle->SetInt("width", v8Rectangle->GetValue("width")->GetIntValue());
      rectangle->SetInt("height",
                        v8Rectangle->GetValue("height")->GetIntValue());
      requestArguments->SetDictionary("rectangle", rectangle);
    }
    request->SetDictionary("arguments", requestArguments);
    CefRefPtr<CefValue> json = CefValue::Create();
    json->SetDictionary(request);
    CefRefPtr<CefProcessMessage> message =
        CefProcessMessage::Create(kOnMouseOverMessage);
    message->GetArgumentList()->SetValue(0, json);
    frame->SendProcessMessage(PID_BROWSER, message);
    return true;
  }

  // Provide the reference counting implementation for this class.
  IMPLEMENT_REFCOUNTING(MouseOverHandler);
};

class OnNavigateHandler : public CefV8Handler {
 public:
  explicit OnNavigateHandler(CefRefPtr<RenderProcessHandler> renderProcessHandler)
      : renderProcessHandler(renderProcessHandler) {}

  virtual bool Execute(const CefString& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception) override {
    if (arguments.empty() || !arguments[0]->IsObject()) {
      return false;
    }

    CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
    CefRefPtr<CefFrame> frame = context->GetFrame();
    CefRefPtr<CefV8Value> event = arguments[0];

    // Ignore canceled navigations
    if (event->GetValue("defaultPrevented")->GetBoolValue()) {
      renderProcessHandler->SendLogMessage(
          frame, LOGSEVERITY_INFO,
          "NavigateEvent ignored: default was prevented");
      return true;
    }

    NavigationEvent navEvent;
    navEvent.source.frameId = frame->GetIdentifier();
    navEvent.source.isMainFrame = frame->IsMain();

    navEvent.transition.type =
        event->GetValue("navigationType")->GetStringValue();
    navEvent.transition.userInitiated =
        event->GetValue("userInitiated")->GetBoolValue();
    navEvent.transition.hashChange =
        event->GetValue("hashChange")->GetBoolValue();
    navEvent.transition.info = CefV8ValueToCefValue(event->GetValue("info"));

    CefRefPtr<CefV8Value> destinationObject = event->GetValue("destination");
    NavigationDestination destination;
    destination.url = destinationObject->GetValue("url")->GetStringValue();
    destination.sameDocument = destinationObject->GetValue("sameDocument")->GetBoolValue();
    destination.key = destinationObject->GetValue("key")->GetStringValue();
    destination.id = destinationObject->GetValue("id")->GetStringValue();
    destination.index = destinationObject->GetValue("index")->GetIntValue();

    CefRefPtr<CefV8Value> getStateFunc =
        destinationObject->GetValue("getState");
    CefV8ValueList emptyArgs;
    destination.state =
        CefV8ValueToCefValue(getStateFunc->ExecuteFunction(destinationObject, emptyArgs));
    navEvent.destination = destination;

    renderProcessHandler->SendNavigationEvent(frame, navEvent);
    return true;
  }

 private:
  CefRefPtr<RenderProcessHandler> renderProcessHandler;

  IMPLEMENT_REFCOUNTING(OnNavigateHandler);
};

class HistoryOverridesHandler : public CefV8Handler {
 public:
  explicit HistoryOverridesHandler(CefRefPtr<RenderProcessHandler> renderProcessHandler)
      : renderProcessHandler(renderProcessHandler) {}

  virtual bool Execute(const CefString& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception) override {
    CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
    CefRefPtr<CefFrame> frame = context->GetFrame();

    int delta = 0;
    if (name == "back") {
      delta = -1;
    } else if (name == "forward") {
      delta = 1;
    } else if (name == "go") {
      if (!arguments.empty() && arguments[0]->IsInt()) {
        delta = arguments[0]->GetIntValue();
      }
    }

    TraverseRequest request;
    request.source.frameId = frame->GetIdentifier();
    request.source.isMainFrame = frame->IsMain();
    request.delta = delta;
    request.key = "";
    request.info = nullptr;
    renderProcessHandler->SendTraverseRequest(frame, request);
    retval = CefV8Value::CreateUndefined();
    return true;
  }

 private:
  CefRefPtr<RenderProcessHandler> renderProcessHandler;

  IMPLEMENT_REFCOUNTING(HistoryOverridesHandler);
};

class NavigationOverridesHandler : public CefV8Handler {
 public:
  explicit NavigationOverridesHandler(
      CefRefPtr<RenderProcessHandler> renderProcessHandler)
      : renderProcessHandler(renderProcessHandler) {}

  virtual bool Execute(const CefString& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception) override {
    CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
    CefRefPtr<CefFrame> frame = context->GetFrame();

    int delta = 0;
    CefString key = "";
    CefRefPtr<CefV8Value> infoVal = nullptr;

    if (name == "back") {
      delta = -1;
      if (!arguments.empty() && arguments[0]->IsObject()) {
        infoVal = arguments[0]->GetValue("info");
      }
    } else if (name == "forward") {
      delta = 1;
      if (!arguments.empty() && arguments[0]->IsObject()) {
        infoVal = arguments[0]->GetValue("info");
      }
    } else if (name == "traverseTo") {
      if (!arguments.empty() && arguments[0]->IsString()) {
        key = arguments[0]->GetStringValue();
      }
      if (arguments.size() > 1 && arguments[1]->IsObject()) {
        infoVal = arguments[1]->GetValue("info");
      }
    }

    TraverseRequest request;
    request.source.frameId = frame->GetIdentifier();
    request.source.isMainFrame = frame->IsMain();
    request.delta = delta;
    request.key = key;
    if (infoVal) {
      request.info = CefV8ValueToCefValue(infoVal);
    }
    renderProcessHandler->SendTraverseRequest(frame, request);

    // Return NavigationResult { committed, finished }
    CefRefPtr<CefV8Value> global = context->GetGlobal();
    CefRefPtr<CefV8Value> navObj = global->GetValue("navigation");
    CefRefPtr<CefV8Value> currentEntry = (navObj && navObj->IsObject())
                                             ? navObj->GetValue("currentEntry")
                                             : nullptr;

    CefRefPtr<CefV8Value> promiseClass = global->GetValue("Promise");
    CefRefPtr<CefV8Value> resolveFunc = promiseClass->GetValue("resolve");
    CefV8ValueList resolveArgs;
    if (currentEntry) {
        resolveArgs.push_back(currentEntry);
    }
    CefRefPtr<CefV8Value> resolvedPromise =
        resolveFunc->ExecuteFunction(promiseClass, resolveArgs);

    retval = CefV8Value::CreateObject(nullptr, nullptr);
    retval->SetValue("committed", resolvedPromise,
                        V8_PROPERTY_ATTRIBUTE_NONE);
    retval->SetValue("finished", resolvedPromise,
                        V8_PROPERTY_ATTRIBUTE_NONE);
    return true;
   }

 private:
  CefRefPtr<RenderProcessHandler> renderProcessHandler;

  IMPLEMENT_REFCOUNTING(NavigationOverridesHandler);
};

class FocusOutHandler : public CefV8Handler {
 public:
  FocusOutHandler() {}

  virtual bool Execute(const CefString& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception) override {
    CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
    CefRefPtr<CefFrame> frame = context->GetFrame();
    CefRefPtr<CefV8Value> event = arguments.front();
    CefRefPtr<CefV8Value> relatedTarget = event->GetValue("relatedTarget");
    CefRefPtr<CefDictionaryValue> request = CefDictionaryValue::Create();
    request->SetString("id", uuid_to_string(CreateUuid()));
    request->SetString("class", "Browser");
    request->SetString("method", "OnMouseOver");
    request->SetInt("instanceId", frame->GetBrowser()->GetIdentifier());
    CefRefPtr<CefDictionaryValue> requestArguments =
        CefDictionaryValue::Create();
    if (!relatedTarget->IsNull()) {
      requestArguments->SetString(
          "tagName", relatedTarget->GetValue("tagName")->GetStringValue());
      CefRefPtr<CefV8Value> attributes = relatedTarget->GetValue("attributes");
      std::string inputType = attributes->GetValue("type")->GetStringValue();
      if (!inputType.empty()) {
        requestArguments->SetString("inputType", inputType);
      }
      requestArguments->SetBool(
          "isEditable", attributes->GetValue("isEditable")->GetBoolValue());
    }
    request->SetDictionary("arguments", requestArguments);
    CefRefPtr<CefValue> json = CefValue::Create();
    json->SetDictionary(request);
    CefRefPtr<CefProcessMessage> message =
        CefProcessMessage::Create(kOnFocusOutMessage);
    message->GetArgumentList()->SetValue(0, json);
    frame->SendProcessMessage(PID_BROWSER, message);
    return true;
  }

  // Provide the reference counting implementation for this class.
  IMPLEMENT_REFCOUNTING(FocusOutHandler);
};

void RenderProcessHandler::OnContextCreated(CefRefPtr<CefBrowser> browser,
                                            CefRefPtr<CefFrame> frame,
                                            CefRefPtr<CefV8Context> context) {
  this->SendLogMessage(frame, LOGSEVERITY_INFO, "Entered OnContextCreated");
  CefRefPtr<CefV8Value> window = context->GetGlobal();

  // mouse over
  CefRefPtr<CefV8Handler> mouseOverHandler = new MouseOverHandler();
  CefV8ValueList mouseOverArguments;
  mouseOverArguments.push_back(CefV8Value::CreateString("mouseover"));
  mouseOverArguments.push_back(
      CefV8Value::CreateFunction("onMouseOver", mouseOverHandler));
  window->GetValue("addEventListener")
      ->ExecuteFunction(window, mouseOverArguments);

  // navigate
  // navigate
  CefRefPtr<CefV8Value> navigation = window->GetValue("navigation");
  if (navigation && navigation->IsObject()) {
    CefRefPtr<CefV8Handler> onNavigateHandler = new OnNavigateHandler(this);
    CefV8ValueList onNavigateArguments;
    onNavigateArguments.push_back(CefV8Value::CreateString("navigate"));
    onNavigateArguments.push_back(
        CefV8Value::CreateFunction("onNavigate", onNavigateHandler));
    navigation->GetValue("addEventListener")
        ->ExecuteFunction(navigation, onNavigateArguments);

    // navigation traverse overrides
    CefRefPtr<CefV8Handler> navigationOverridesHandler =
        new NavigationOverridesHandler(this);
    navigation->SetValue(
        "back", CefV8Value::CreateFunction("back", navigationOverridesHandler),
        V8_PROPERTY_ATTRIBUTE_NONE);
    navigation->SetValue(
        "forward",
        CefV8Value::CreateFunction("forward", navigationOverridesHandler),
        V8_PROPERTY_ATTRIBUTE_NONE);
    navigation->SetValue(
        "traverseTo",
        CefV8Value::CreateFunction("traverseTo", navigationOverridesHandler),
        V8_PROPERTY_ATTRIBUTE_NONE);
  }

  // history traverse overrides
  CefRefPtr<CefV8Value> history = window->GetValue("history");
  if (history && history->IsObject()) {
    CefRefPtr<CefV8Handler> historyOverridesHandler =
        new HistoryOverridesHandler(this);
    history->SetValue(
        "back", CefV8Value::CreateFunction("back", historyOverridesHandler),
        V8_PROPERTY_ATTRIBUTE_NONE);
    history->SetValue(
        "forward",
        CefV8Value::CreateFunction("forward", historyOverridesHandler),
        V8_PROPERTY_ATTRIBUTE_NONE);
    history->SetValue("go",
                      CefV8Value::CreateFunction("go", historyOverridesHandler),
                      V8_PROPERTY_ATTRIBUTE_NONE);
  }

  // focus out
  CefRefPtr<CefV8Value> document = window->GetValue("document");
  CefRefPtr<CefV8Handler> focusOutHandler = new FocusOutHandler();
  CefV8ValueList focusOutArguments;
  focusOutArguments.push_back(CefV8Value::CreateString("focusout"));
  focusOutArguments.push_back(
      CefV8Value::CreateFunction("onFocusOut", focusOutHandler));
  document->GetValue("addEventListener")
      ->ExecuteFunction(document, focusOutArguments);
}

void RenderProcessHandler::OnContextReleased(CefRefPtr<CefBrowser> browser,
                                             CefRefPtr<CefFrame> frame,
                                             CefRefPtr<CefV8Context> context) {}

void RenderProcessHandler::OnUncaughtException(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefV8Context> context,
    CefRefPtr<CefV8Exception> exception,
    CefRefPtr<CefV8StackTrace> stackTrace) {}

void RenderProcessHandler::OnFocusedNodeChanged(CefRefPtr<CefBrowser> browser,
                                                CefRefPtr<CefFrame> frame,
                                                CefRefPtr<CefDOMNode> node) {
  if (node.get()) {
    CefRefPtr<CefDictionaryValue> request = CefDictionaryValue::Create();
    request->SetString("id", uuid_to_string(CreateUuid()));
    request->SetString("class", "Browser");
    request->SetString("method", "OnFocusedNodeChanged");
    request->SetInt("instanceId", browser->GetIdentifier());
    CefRefPtr<CefDictionaryValue> arguments = CefDictionaryValue::Create();
    arguments->SetString("tagName", node->GetElementTagName());
    std::string inputType = node->GetElementAttribute("type");
    if (!inputType.empty()) {
      arguments->SetString("inputType", inputType);
    }
    arguments->SetBool("isEditable", node->IsEditable());
    request->SetDictionary("arguments", arguments);
    CefRefPtr<CefValue> json = CefValue::Create();
    json->SetDictionary(request);
    CefRefPtr<CefProcessMessage> message =
        CefProcessMessage::Create(kOnNavigationMessage);
    message->GetArgumentList()->SetValue(0, json);
    frame->SendProcessMessage(PID_BROWSER, message);
  }
}

// Custom handler for our promise "then" callback
class PromiseThenHandler : public CefV8Handler {
 public:
  explicit PromiseThenHandler(CefRefPtr<CefFrame> frame,
                              CefProcessId sourceProcessId,
                              const CefString& requestId)
      : frame(frame), sourceProcessId(sourceProcessId), requestId(requestId) {}

  bool Execute(const CefString& name,
               CefRefPtr<CefV8Value> object,
               const CefV8ValueList& arguments,
               CefRefPtr<CefV8Value>& retval,
               CefString& exception) override {
    if (name != "onPromiseResolved" || arguments.size() == 0) {
      return false;
    }

    CefRefPtr<CefProcessMessage> responseMessage =
        CefProcessMessage::Create(kOnEvalMessage);
    CefRefPtr<CefDictionaryValue> messageArguments =
        CefDictionaryValue::Create();
    CefRefPtr<CefDictionaryValue> response = CefDictionaryValue::Create();
    response->SetString("id", requestId);
    response->SetBool("success", true);
    response->SetValue("result", CefV8ValueToCefValue(arguments[0]));
    CefRefPtr<CefValue> json = CefValue::Create();
    json->SetDictionary(response);
    responseMessage->GetArgumentList()->SetValue(0, json);
    frame->SendProcessMessage(sourceProcessId, responseMessage);
    retval = arguments[0];
    return true;
  }

 private:
  CefRefPtr<CefFrame> frame;
  CefProcessId sourceProcessId;
  CefString requestId;
  IMPLEMENT_REFCOUNTING(PromiseThenHandler);
};

bool RenderProcessHandler::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
  DCHECK_EQ(source_process, PID_BROWSER);
  CefRefPtr<CefV8Context> context = frame->GetV8Context();
  context->Enter();
  const CefString& name = message->GetName();
  bool handled = false;
  if (name == "Eval") {
    auto argumentList = message->GetArgumentList();
    const CefString& requestId = argumentList->GetString(0);
    const CefString& code = argumentList->GetString(1);
    const CefString& scriptUrl = argumentList->GetString(2);
    const int startLine = argumentList->GetInt(3);
    CefRefPtr<CefV8Value> retval;
    CefRefPtr<CefV8Exception> exception;
    bool success = context->Eval(code, scriptUrl, startLine, retval, exception);
    if (success && retval->IsPromise()) {
      CefRefPtr<CefV8Value> thenFunction = retval->GetValue("then");
      CefRefPtr<PromiseThenHandler> handler =
          new PromiseThenHandler(frame, source_process, requestId);
      CefRefPtr<CefV8Value> onResolvedFunc =
          CefV8Value::CreateFunction("onPromiseResolved", handler);
      thenFunction->ExecuteFunction(retval, {onResolvedFunc});
    } else {
      CefRefPtr<CefDictionaryValue> response = CefDictionaryValue::Create();
      response->SetString("id", requestId);
      response->SetBool("success", success);
      if (success) {
        response->SetValue("result", CefV8ValueToCefValue(retval));
      } else {
        CefRefPtr<CefDictionaryValue> error = CefDictionaryValue::Create();
        error->SetInt("endColumn", exception->GetEndColumn());
        error->SetInt("endPosition", exception->GetEndPosition());
        error->SetInt("lineNumber", exception->GetLineNumber());
        error->SetString("message", exception->GetMessage());
        error->SetString("scriptResourceName",
                         exception->GetScriptResourceName());
        error->SetString("sourceLine", exception->GetSourceLine());
        error->SetInt("startColumn", exception->GetStartColumn());
        error->SetInt("startPosition", exception->GetStartPosition());
        response->SetDictionary("error", error);
      }
      CefRefPtr<CefProcessMessage> responseMessage =
          CefProcessMessage::Create(kOnEvalMessage);
      CefRefPtr<CefValue> json = CefValue::Create();
      json->SetDictionary(response);
      responseMessage->GetArgumentList()->SetValue(0, json);
      frame->SendProcessMessage(source_process, responseMessage);
    }
    handled = true;
  }
  context->Exit();
  return handled;
}