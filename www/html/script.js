// Wait for DOM to be fully loaded
document.addEventListener('DOMContentLoaded', function() {
    
    // Initialize all functionality
    initSmoothScrolling();
    initNavigationHighlighting();
    initFormValidation();
    initInteractiveElements();
    initScrollToTop();
    initTypingEffect();
    initNewsToggle();
    
    console.log('Website JavaScript loaded successfully!');
});

// Smooth scrolling for navigation links
function initSmoothScrolling() {
    const navLinks = document.querySelectorAll('nav a[href^="#"]');
    
    navLinks.forEach(link => {
        link.addEventListener('click', function(e) {
            e.preventDefault();
            
            const targetId = this.getAttribute('href');
            const targetSection = document.querySelector(targetId);
            
            if (targetSection) {
                const offsetTop = targetSection.offsetTop - 80; // Account for sticky nav
                
                window.scrollTo({
                    top: offsetTop,
                    behavior: 'smooth'
                });
            }
        });
    });
}

// Highlight active navigation item based on scroll position
function initNavigationHighlighting() {
    const sections = document.querySelectorAll('section[id]');
    const navLinks = document.querySelectorAll('nav a[href^="#"]');
    
    function highlightNavigation() {
        let current = '';
        
        sections.forEach(section => {
            const sectionTop = section.offsetTop - 100;
            const sectionHeight = section.offsetHeight;
            
            if (window.scrollY >= sectionTop && window.scrollY < sectionTop + sectionHeight) {
                current = section.getAttribute('id');
            }
        });
        
        navLinks.forEach(link => {
            link.classList.remove('active');
            if (link.getAttribute('href') === '#' + current) {
                link.classList.add('active');
            }
        });
    }
    
    window.addEventListener('scroll', highlightNavigation);
    
    // Add CSS for active navigation
    const style = document.createElement('style');
    style.textContent = `
        nav a.active {
            background-color: #3498db !important;
            transform: scale(1.05);
        }
    `;
    document.head.appendChild(style);
}

// Form validation and submission
function initFormValidation() {
    const form = document.querySelector('form');
    const nameInput = document.getElementById('name');
    const emailInput = document.getElementById('email');
    const subjectInput = document.getElementById('subject');
    const messageInput = document.getElementById('message');
    const submitButton = document.querySelector('input[type="submit"]');
    
    if (!form) return;
    
    // Real-time validation
    nameInput.addEventListener('input', validateName);
    emailInput.addEventListener('input', validateEmail);
    subjectInput.addEventListener('input', validateSubject);
    messageInput.addEventListener('input', validateMessage);
    
    // Form submission
    form.addEventListener('submit', function(e) {
        e.preventDefault();
        
        if (validateForm()) {
            submitForm();
        }
    });
    
    function validateName() {
        const name = nameInput.value.trim();
        if (name.length < 2) {
            showError(nameInput, 'Name must be at least 2 characters');
            return false;
        }
        showSuccess(nameInput);
        return true;
    }
    
    function validateEmail() {
        const email = emailInput.value.trim();
        const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
        
        if (!emailRegex.test(email)) {
            showError(emailInput, 'Please enter a valid email address');
            return false;
        }
        showSuccess(emailInput);
        return true;
    }
    
    function validateSubject() {
        const subject = subjectInput.value.trim();
        if (subject.length < 5) {
            showError(subjectInput, 'Subject must be at least 5 characters');
            return false;
        }
        showSuccess(subjectInput);
        return true;
    }
    
    function validateMessage() {
        const message = messageInput.value.trim();
        if (message.length < 10) {
            showError(messageInput, 'Message must be at least 10 characters');
            return false;
        }
        showSuccess(messageInput);
        return true;
    }
    
    function validateForm() {
        return validateName() && validateEmail() && validateSubject() && validateMessage();
    }
    
    function showError(input, message) {
        clearMessages(input);
        input.style.borderColor = '#e74c3c';
        
        const errorDiv = document.createElement('div');
        errorDiv.className = 'error-message';
        errorDiv.textContent = message;
        errorDiv.style.color = '#e74c3c';
        errorDiv.style.fontSize = '0.9rem';
        errorDiv.style.marginTop = '0.5rem';
        
        input.parentNode.appendChild(errorDiv);
    }
    
    function showSuccess(input) {
        clearMessages(input);
        input.style.borderColor = '#27ae60';
    }
    
    function clearMessages(input) {
        input.style.borderColor = '#ddd';
        const existingMessage = input.parentNode.querySelector('.error-message, .success-message');
        if (existingMessage) {
            existingMessage.remove();
        }
    }
    
    function submitForm() {
        // Simulate form submission
        submitButton.disabled = true;
        submitButton.value = 'Sending...';
        
        setTimeout(() => {
            alert('Thank you! Your message has been sent successfully.');
            form.reset();
            
            // Clear all validation styles
            [nameInput, emailInput, subjectInput, messageInput].forEach(input => {
                clearMessages(input);
            });
            
            submitButton.disabled = false;
            submitButton.value = 'Send Message';
        }, 2000);
    }
}

// Interactive elements and animations
function initInteractiveElements() {
    // Add hover effects to table rows
    const tableRows = document.querySelectorAll('tbody tr');
    tableRows.forEach(row => {
        row.addEventListener('mouseenter', function() {
            this.style.transform = 'scale(1.02)';
            this.style.transition = 'transform 0.2s ease';
        });
        
        row.addEventListener('mouseleave', function() {
            this.style.transform = 'scale(1)';
        });
    });
    
    // Add click animation to buttons
    const buttons = document.querySelectorAll('input[type="submit"], button');
    buttons.forEach(button => {
        button.addEventListener('click', function() {
            this.style.transform = 'scale(0.95)';
            setTimeout(() => {
                this.style.transform = 'scale(1)';
            }, 150);
        });
    });
    
    // Add loading animation to external links
    const externalLinks = document.querySelectorAll('a[target="_blank"]');
    externalLinks.forEach(link => {
        link.addEventListener('click', function() {
            const originalText = this.textContent;
            this.textContent = 'Opening...';
            
            setTimeout(() => {
                this.textContent = originalText;
            }, 2000);
        });
    });
}

// Scroll to top functionality
function initScrollToTop() {
    // Create scroll to top button
    const scrollToTopBtn = document.createElement('button');
    scrollToTopBtn.innerHTML = '↑';
    scrollToTopBtn.className = 'scroll-to-top';
    scrollToTopBtn.style.cssText = `
        position: fixed;
        bottom: 20px;
        right: 20px;
        width: 50px;
        height: 50px;
        border: none;
        border-radius: 50%;
        background: linear-gradient(135deg, #3498db 0%, #2980b9 100%);
        color: white;
        font-size: 20px;
        cursor: pointer;
        opacity: 0;
        transition: all 0.3s ease;
        z-index: 1000;
        box-shadow: 0 4px 10px rgba(52, 152, 219, 0.3);
    `;
    
    document.body.appendChild(scrollToTopBtn);
    
    // Show/hide button based on scroll position
    window.addEventListener('scroll', function() {
        if (window.scrollY > 300) {
            scrollToTopBtn.style.opacity = '1';
            scrollToTopBtn.style.transform = 'translateY(0)';
        } else {
            scrollToTopBtn.style.opacity = '0';
            scrollToTopBtn.style.transform = 'translateY(10px)';
        }
    });
    
    // Scroll to top when clicked
    scrollToTopBtn.addEventListener('click', function() {
        window.scrollTo({
            top: 0,
            behavior: 'smooth'
        });
    });
    
    // Hover effects
    scrollToTopBtn.addEventListener('mouseenter', function() {
        this.style.transform = 'scale(1.1)';
    });
    
    scrollToTopBtn.addEventListener('mouseleave', function() {
        this.style.transform = 'scale(1)';
    });
}

// Typing effect for header
function initTypingEffect() {
    const headerP = document.querySelector('header p');
    if (!headerP) return;
    
    const originalText = headerP.textContent;
    headerP.textContent = '';
    
    let i = 0;
    const typeWriter = () => {
        if (i < originalText.length) {
            headerP.textContent += originalText.charAt(i);
            i++;
            setTimeout(typeWriter, 50);
        }
    };
    
    // Start typing effect after page loads
    setTimeout(typeWriter, 1000);
}

// Toggle news articles
function initNewsToggle() {
    const articles = document.querySelectorAll('article');
    
    articles.forEach(article => {
        const title = article.querySelector('h4');
        const content = article.querySelector('p');
        
        if (title && content) {
            // Initially hide content
            content.style.display = 'none';
            title.style.cursor = 'pointer';
            title.style.userSelect = 'none';
            title.title = 'Click to expand';
            
            // Add expand/collapse indicator
            const indicator = document.createElement('span');
            indicator.textContent = ' [+]';
            indicator.style.color = '#3498db';
            title.appendChild(indicator);
            
            title.addEventListener('click', function() {
                if (content.style.display === 'none') {
                    content.style.display = 'block';
                    indicator.textContent = ' [-]';
                    title.title = 'Click to collapse';
                    
                    // Smooth expand animation
                    content.style.opacity = '0';
                    content.style.transform = 'translateY(-10px)';
                    content.style.transition = 'all 0.3s ease';
                    
                    setTimeout(() => {
                        content.style.opacity = '1';
                        content.style.transform = 'translateY(0)';
                    }, 10);
                } else {
                    content.style.display = 'none';
                    indicator.textContent = ' [+]';
                    title.title = 'Click to expand';
                }
            });
        }
    });
}

// Utility functions
function debounce(func, wait) {
    let timeout;
    return function executedFunction(...args) {
        const later = () => {
            clearTimeout(timeout);
            func(...args);
        };
        clearTimeout(timeout);
        timeout = setTimeout(later, wait);
    };
}

// Add some fun Easter eggs
document.addEventListener('keydown', function(e) {
    // Konami code: ↑↑↓↓←→←→BA
    const konamiCode = [38, 38, 40, 40, 37, 39, 37, 39, 66, 65];
    window.konamiKeys = window.konamiKeys || [];
    
    window.konamiKeys.push(e.keyCode);
    
    if (window.konamiKeys.length > konamiCode.length) {
        window.konamiKeys.shift();
    }
    
    if (window.konamiKeys.join(',') === konamiCode.join(',')) {
        document.body.style.animation = 'rainbow 2s infinite';
        
        const style = document.createElement('style');
        style.textContent = `
            @keyframes rainbow {
                0% { filter: hue-rotate(0deg); }
                100% { filter: hue-rotate(360deg); }
            }
        `;
        document.head.appendChild(style);
        
        setTimeout(() => {
            document.body.style.animation = '';
        }, 4000);
        
        window.konamiKeys = [];
    }
});

// Performance monitoring
window.addEventListener('load', function() {
    const loadTime = performance.now();
    console.log(`Page loaded in ${loadTime.toFixed(2)}ms`);
    
    // Optional: Send analytics
    // analytics.track('page_load_time', { time: loadTime });
});
