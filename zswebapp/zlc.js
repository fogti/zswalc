"use strict";

var lcid_up = undefined;
var lcid_down = undefined;
var do_notif = false;
// show_chat can be used to manually override 'lcid_up'
// and prevent further scrolling/paging.
var show_chat = 'lower_bound=cur';
var chat_reloader = undefined;
var notif_is_active = false;
var chatelem = undefined;
var max_msgcnt = 20;

if("Notification" in window) {
  if(Notification.permission === 'granted')
    do_notif = true;
  else if(Notification.permission !== 'denied') {
    Notification.requestPermission(function(permission) {
      if(permission === "granted") do_notif = true;
    });
  }
}

window.onpopstate = function(event) {
  if(event.state) {
    change_chatseek(event.state.show_chat, false);
  }
};

function is_latest_chat() {
  return show_chat === 'lower_bound=cur';
}

function loadchat() {
  let xhr = new XMLHttpRequest();
  xhr.responseType = 'text';
  let ctgp = show_chat;
  if(is_latest_chat() && lcid_up) {
    ctgp = 'lower_bound=' + lcid_up;
  }
  xhr.onload = function(event) {
    if(xhr.status == 304) {
      return;
    }
    if(xhr.status < 200 || xhr.status >= 300) {
      console.warn(xhr.statusText, xhr.responseText);
      return;
    }
    if(xhr.responseText) {
      chatelem.innerHTML = xhr.responseText + chatelem.innerHTML;
      if(lcid_up && do_notif && is_latest_chat() && !notif_is_active) {
        notif_is_active = true;
        let notif = new Notification("ZSChat Nachricht");
        notif.onclose = function(event) {
          notif_is_active = false;
        };
      }
    }
    if(!lcid_up || ctgp.startsWith('lower_bound=')) {
      lcid_up = xhr.getResponseHeader('X-LastMsgId');
    }
    if(!lcid_down || ctgp.startsWith('upper_bound=')) {
      lcid_down = xhr.getResponseHeader('X-FirstMsgId');
    }
  };
  xhr.open('GET', '?'+ctgp);
  xhr.send();
}

function change_chatseek(new_chat, push_hist) {
  if(show_chat != new_chat) {
    show_chat = new_chat;
    if(push_hist) {
      let pgtitle = 'Chat';
      if(!is_latest_chat()) {
        pgtitle += ' - ' + show_chat;
      }
      window.history.pushState({ "show_chat": show_chat }, pgtitle, "?show_chat=" + show_chat);
    }
    initchat();
  }
}

function sendchatmsg(event) {
  // always prevent send
  event.preventDefault();
  let dat = document.getElementById('in').value;
  if(dat === '') {
    return;
  }
  document.getElementById('in').value = '';

  let xhr = new XMLHttpRequest();
  xhr.responseType = 'text';
  xhr.onload = function(event) {
    // log errors and warnings
    if(xhr.status < 200 || xhr.status >= 300) {
      console.warn(xhr.statusText, xhr.responseText);
      return;
    }
    // trigger reload
    loadchat();
  };
  xhr.open('POST', '');
  xhr.send(dat);
}

function initchat() {
  if(!chatelem) {
    chatelem = document.getElementById('chat');
    if(document.show_chat) {
      show_chat = document.show_chat;
      document.show_chat = undefined;
    }
  }
  if(is_latest_chat() != chat_reloader) {
    if(chat_reloader) {
      clearInterval(chat_reloader);
      chat_reloader = undefined;
    } else {
      chat_reloader = setInterval(loadchat, 30000);
    }
  }
  chatelem.innerHTML = '';
  lcid_up = undefined;
  lcid_down = undefined;
  loadchat();
}

function nextchat(event) {
  event.preventDefault();
  if(is_latest_chat()) {
    loadchat();
    return;
  }
  if(!lcid_up) {
    console.warn('unable to select next chat');
    return;
  }
  change_chatseek('lower_bound=' + lcid_up, true);
}

function prevchat(event) {
  event.preventDefault();
  if(!lcid_down) {
    console.warn('unable to select previous chat');
    return;
  }
  change_chatseek('upper_bound=' + lcid_down, true);
}
