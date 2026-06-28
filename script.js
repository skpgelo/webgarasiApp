// ISI KREDENSIAL FIREBASE ANDA DI SINI
const firebaseConfig = {
    apiKey: "API_KEY_ANDA",
    authDomain: "PROJECT_://firebaseapp.com",
    databaseURL: "https://PROJECT_://firebaseio.com",
    projectId: "PROJECT_ID",
    storageBucket: "PROJECT_://appspot.com",
    messagingSenderId: "SENDER_ID",
    appId: "APP_ID"
};

// Inisialisasi Firebase
firebase.initializeApp(firebaseConfig);
const auth = firebase.auth();
const database = firebase.database();

let currentStates = { R1: 0, R2: 0, R3: 0, R4: 0 };
let dbRefs = {};
let lastSeenValue = 0;
let heartbeatInterval;

// 1. MONITORING STATUS OTENTIKASI USER
auth.onAuthStateChanged((user) => {
    const loginSection = document.getElementById('loginSection');
    const dashboardSection = document.getElementById('dashboardSection');

    if (user) {
        loginSection.classList.add('hidden');
        dashboardSection.classList.remove('hidden');
        initFirebaseRealtime();
    } else {
        loginSection.classList.remove('hidden');
        dashboardSection.classList.add('hidden');
        detachFirebaseRealtime();
    }
});

// 2. PROSES LOG IN + REMEMBER ME
document.getElementById('loginForm').addEventListener('submit', (e) => {
    e.preventDefault();
    const email = document.getElementById('email').value;
    const password = document.getElementById('password').value;
    const isRememberMe = document.getElementById('rememberMe').checked;
    const errorDiv = document.getElementById('loginError');
    const btnLogin = document.getElementById('btnLogin');

    errorDiv.classList.add('hidden');
    btnLogin.disabled = true;
    btnLogin.innerText = "Memproses...";

    // Mengubah persistent session berdasarkan ceklis 'Remember Me'
    const persistenceType = isRememberMe ? firebase.auth.Auth.Persistence.LOCAL : firebase.auth.Auth.Persistence.SESSION;

    auth.setPersistence(persistenceType)
        .then(() => {
            return auth.signInWithEmailAndPassword(email, password);
        })
        .then(() => {
            btnLogin.disabled = false;
            btnLogin.innerText = "Masuk ke Dashboard";
        })
        .catch((error) => {
            btnLogin.disabled = false;
            btnLogin.innerText = "Masuk ke Dashboard";
            errorDiv.classList.remove('hidden');
            
            if (error.code === 'auth/wrong-password' || error.code === 'auth/user-not-found') {
                errorDiv.innerText = "Email atau Password salah!";
            } else if (error.code === 'auth/invalid-email') {
                errorDiv.innerText = "Format email tidak valid.";
            } else {
                errorDiv.innerText = error.message;
            }
        });
});

// 3. PROSES LOGOUT
function logout() {
    auth.signOut();
}

// 4. SINKRONISASI REALTIME DATABASE
function initFirebaseRealtime() {
    // Mendengarkan data Relay R1 - R4
    const relayPaths = ['R1', 'R2', 'R3', 'R4'];
    relayPaths.forEach(relay => {
        dbRefs[relay] = database.ref('/' + relay);
        dbRefs[relay].on('value', (snapshot) => {
            const val = snapshot.val() || 0;
            currentStates[relay] = val;
            updateButtonUI(relay, val);
        });
    });

    // Mendengarkan Heartbeat dari ESP8266
    dbRefs['last_seen'] = database.ref('/status/last_seen');
    dbRefs['last_seen'].on('value', (snapshot) => {
        lastSeenValue = snapshot.val() || 0;
        updateConnectionUI(true);
    });

    // Pengecekan timeout berkala (Offline jika 12 detik tidak ada detak)
    let localCheckCounter = 0;
    let lastLoggedValue = 0;
    
    heartbeatInterval = setInterval(() => {
        if (lastSeenValue === lastLoggedValue) {
            localCheckCounter += 4;
        } else {
            localCheckCounter = 0;
            lastLoggedValue = lastSeenValue;
        }

        if (localCheckCounter >= 12) {
            updateConnectionUI(false);
        } else {
            updateConnectionUI(true);
        }
    }, 4000);
}

// MEMUTUSKAN OBSERVER DATA
function detachFirebaseRealtime() {
    const relayPaths = ['R1', 'R2', 'R3', 'R4', 'last_seen'];
    relayPaths.forEach(relay => {
        if (dbRefs[relay]) dbRefs[relay].off();
    });
    if (heartbeatInterval) clearInterval(heartbeatInterval);
}

// 5. UPDATE TAMPILAN TEXT VISUAL KONEKSI ALAT
function updateConnectionUI(isOnline) {
    const badge = document.getElementById('deviceStatus');
    if (isOnline && lastSeenValue > 0) {
        badge.className = "status-badge bg-success text-white";
        badge.innerText = "● Terhubung";
    } else {
        badge.className = "status-badge bg-danger text-white";
        badge.innerText = "○ Terputus";
    }
}

// 6. UPDATE WARNA DAN TEKS TOMBOL DASHBOARD
function updateButtonUI(relay, val) {
    const btn = document.getElementById('btn' + relay);
    if (val === 1) {
        btn.className = "toggle-btn btn-on";
        btn.innerText = "OFF"; 
    } else {
        btn.className = "toggle-btn btn-off";
        btn.innerText = "ON"; 
    }
}

// 7. INTERLOCK & KLIK TRANSMISI DATA KE FIREBASE
function toggleRelay(relay) {
    let newValue = currentStates[relay] === 1 ? 0 : 1;
    
    if (relay === 'R1' && newValue === 1) {
        database.ref('/').update({ "R1": 1, "R2": 0 });
    } else if (relay === 'R2' && newValue === 1) {
        database.ref('/').update({ "R1": 0, "R2": 1 });
    } else {
        database.ref('/' + relay).set(newValue);
    }
}
