package cmd

import (
	"fmt"
	"log"
	"net/http"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/gorilla/websocket"
	"github.com/urfave/cli/v2"
	"go.bug.st/serial"
)

// EventEmitter is a simple event emitter.
type EventEmitter struct {
	listeners map[string][]chan string
	mu        sync.Mutex
}

// NewEventEmitter creates a new EventEmitter.
func NewEventEmitter() *EventEmitter {
	return &EventEmitter{
		listeners: make(map[string][]chan string),
	}
}

// On registers a listener for an event.
func (e *EventEmitter) On(event string, ch chan string) {
	e.mu.Lock()

	defer e.mu.Unlock()

	e.listeners[event] = append(e.listeners[event], ch)
}

// Off removes a listener for an event.
func (e *EventEmitter) Off(event string, ch chan string) {
	e.mu.Lock()

	defer e.mu.Unlock()

	listeners := e.listeners[event]

	for i, listener := range listeners {
		if listener == ch {
			e.listeners[event] = append(listeners[:i], listeners[i+1:]...)
			break
		}
	}
}

// Emit emits an event to all listeners.
func (e *EventEmitter) Emit(event string, msg string) {
	e.mu.Lock()

	defer e.mu.Unlock()

	for _, ch := range e.listeners[event] {
		select {
		case ch <- msg:
		default:
		}
	}
}

var upgrader = websocket.Upgrader{
	ReadBufferSize:  1024,
	WriteBufferSize: 1024,
	CheckOrigin: func(r *http.Request) bool {
		return true
	},
}

var connections = make(map[*websocket.Conn]chan string)
var connectionsMu sync.Mutex
var isDebugMode = false
var incomingMessage = ""

func handleConnections(w http.ResponseWriter, r *http.Request, emitter *EventEmitter) {
	conn, err := upgrader.Upgrade(w, r, nil)

	if err != nil {
		fmt.Println(err)
		return
	}

	defer conn.Close()

	messageChan := make(chan string)
	connectionsMu.Lock()

	connections[conn] = messageChan
	connectionsMu.Unlock()

	emitter.On("keypress", messageChan)

	go func() {
		for msg := range messageChan {
			err := conn.WriteMessage(websocket.TextMessage, []byte(msg))

			if err != nil {
				fmt.Println("write:", err)
				conn.Close()
				connectionsMu.Lock()

				delete(connections, conn)

				connectionsMu.Unlock()

				emitter.Off("keypress", messageChan)

				return
			}
		}
	}()

	for {
		_, _, err := conn.ReadMessage()

		if err != nil {
			fmt.Println("read:", err)
			connectionsMu.Lock()

			delete(connections, conn)
			connectionsMu.Unlock()

			close(messageChan)

			emitter.Off("keypress", messageChan)
			break
		}
	}
}

func StartWebsocketServer(cCtx *cli.Context) error {
	serverPort := cCtx.Int("port")
	arduinoPort := cCtx.String("board-port")
	arduinoBaudRate := cCtx.Int("baud-rate")
	isDebugMode = cCtx.Bool("debug")

	arduinoMode := &serial.Mode{
		BaudRate: arduinoBaudRate,
	}

	if len(arduinoPort) == 0 {
		fmt.Println("Please specify the Arduino port!")
		return nil
	}

	port, arduErr := serial.Open(arduinoPort, arduinoMode)
	if arduErr != nil {
		fmt.Println("Error connecting to Arduino board:", arduErr)
		return nil
	}

	buff := make([]byte, 32)

	// EventEmitter to broadcast messages to WebSocket clients
	emitter := NewEventEmitter()

	// Start WebSocket server
	go func() {
		http.HandleFunc("/ws", func(w http.ResponseWriter, r *http.Request) {
			handleConnections(w, r, emitter)
		})

		fmt.Println("WebSocket server started on 127.0.0.1:" + strconv.Itoa(serverPort))

		err := http.ListenAndServe("127.0.0.1:"+strconv.Itoa(serverPort), nil)

		if err != nil {
			fmt.Println("ListenAndServe:", err)
		}
	}()

	for {
		n, err := port.Read(buff)

		if err != nil {
			log.Fatal("Error reading data:", err)
			break
		}

		incomingData := strings.TrimSpace(string(buff[:n]))

		if strings.EqualFold(incomingData, "") {
			continue
		}

		if isDebugMode {
			dt := time.Now()
			fmt.Println("[", dt.Format(time.StampMilli), "]", incomingData)
		}

		// Implement start bit with < and the > character as a stop bit.
		// Ensuring stable and consistent data without normalizer.
		for i := 0; i < len(incomingData); i++ {
			char := string(incomingData[i])

			if char == "<" {
				incomingMessage = ""
			} else if char == ">" {
				if isDebugMode {
					dt := time.Now()
					fmt.Println("[", dt.Format(time.StampMilli), "]", incomingMessage)
				}

				// Emit the normalized keybind message to WebSocket clients
				if strings.HasPrefix(incomingMessage, "SORA-") {
					emitter.Emit("keypress", incomingMessage)
				}

				// Send an acknowledgement
				port.Write([]byte("A\n\r"))

				// Reset value
				incomingMessage = ""
			} else {
				incomingMessage += char
			}
		}
	}

	return nil
}
