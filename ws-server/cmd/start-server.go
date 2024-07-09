package cmd

import (
	"fmt"
	"log"
	"net/http"
	"strconv"
	"strings"
	"sync"

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

	buff := make([]byte, 30)

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

		if isDebugMode {
			fmt.Println(n)
			fmt.Println(buff)
		}

		if err != nil {
			log.Fatal("Error reading data:", err)
			break
		}

		if n == 0 {
			fmt.Println("\nEOF")
			break
		}

		incomingArduinoData := strings.Split(string(buff[:n]), "\r\n")
		santizedNewLine := incomingArduinoData[0]

		if isDebugMode {
			fmt.Println(incomingArduinoData)
			fmt.Println(santizedNewLine)
		}

		if len(santizedNewLine) != 0 {
			// Fix for Windows reading partial data from Arduino
			if strings.HasPrefix(santizedNewLine, "SORA-KEYBIND-") || strings.HasPrefix(santizedNewLine, "ORA-KEYBIND-") {
				// Normalize the keybind
				normalizedKeybind := strings.Split(santizedNewLine, "-KEYBIND-")
				output := "SORA-KEYBIND-" + normalizedKeybind[1]

				if isDebugMode {
					fmt.Println("normalized keybind:", normalizedKeybind)
					fmt.Println("output:", output)
				}

				// Emit the normalized keybind message to WebSocket clients
				emitter.Emit("keypress", output)
			}
		}
	}

	return nil
}
