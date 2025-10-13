const API_URL = `${window.location.protocol}//${window.location.host}/api/users`;

let allUsers = [];
let currentPage = 1;
const PAGE_SIZE = 10;

window.addEventListener("DOMContentLoaded", loadUsers);

async function loadUsers() {
    try {
        const res = await fetch(API_URL);
        allUsers = await res.json();
        currentPage = 1;
        renderUsers();
        renderPagination();
    } catch (err) {
        alert("Error loading users: " + err.message);
    }
}

function renderUsers() {
    const tbody = document.querySelector("#usersTable tbody");
    tbody.innerHTML = "";

    const start = (currentPage - 1) * PAGE_SIZE;
    const end = start + PAGE_SIZE;
    const usersToShow = allUsers.slice(start, end);

    usersToShow.forEach(user => {
        const row = document.createElement("tr");

        row.innerHTML = `
            <td>${user.id}</td>
            <td>${user.name}</td>
            <td>${user.email}</td>
            <td class="actions">
                <button onclick="editUser(${user.id}, '${user.name}', '${user.email}')">Edit</button>
                <button class="delete" onclick="deleteUser(${user.id})">Delete</button>
            </td>
        `;
        tbody.appendChild(row);
    });
}

function renderPagination() {
    const totalPages = Math.ceil(allUsers.length / PAGE_SIZE);
    const paginationDiv = document.getElementById("pagination");

    paginationDiv.innerHTML = `
        <button onclick="changePage(-1)" ${currentPage === 1 ? "disabled" : ""}>⬅ Prev</button>
        Page ${currentPage} of ${totalPages}
        <button onclick="changePage(1)" ${currentPage === totalPages ? "disabled" : ""}>Next ➡</button>
    `;
}

function changePage(direction) {
    const totalPages = Math.ceil(allUsers.length / PAGE_SIZE);
    currentPage += direction;
    if (currentPage < 1) currentPage = 1;
    if (currentPage > totalPages) currentPage = totalPages;
    renderUsers();
    renderPagination();
}

document.getElementById("userForm").addEventListener("submit", async (e) => {
    e.preventDefault();
    const id = document.getElementById("userId").value;
    const name = document.getElementById("name").value;
    const email = document.getElementById("email").value;

    try {
        const options = {
            method: id ? "PUT" : "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ name, email }),
        };

        await fetch(id ? `${API_URL}/${id}` : API_URL, options);
        resetForm();
        await loadUsers();
    } catch (err) {
        alert("Error saving user: " + err.message);
    }
});

function editUser(id, name, email) {
    document.getElementById("userId").value = id;
    document.getElementById("name").value = name;
    document.getElementById("email").value = email;
}

async function deleteUser(id) {
    if (!confirm("Delete this user?")) return;
    try {
        await fetch(`${API_URL}/${id}`, { method: "DELETE" });
        await loadUsers();
    } catch (err) {
        alert("Error deleting user: " + err.message);
    }
}

function resetForm() {
    document.getElementById("userId").value = "";
    document.getElementById("name").value = "";
    document.getElementById("email").value = "";
}
